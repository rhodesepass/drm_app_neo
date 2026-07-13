//
// drm_warpper 的 PC/SDL 后端 —— 与 drm_warpper.c 二选一编译（链接期选择后端）。
//
// 复刻设备语义：
//   - 三个图层（video NV12 / overlay ARGB8888 / ui RGB565）自下而上合成，
//     等价 sunxi DEBE 的 plane 叠加；coord/alpha/几何裁缩全部支持
//   - display_queue/free_queue 的 item 协议原样保留（mediaplayer 依赖
//     free_queue 回流解除帧占用）。区别：PC 上 FLIP 帧在翻页时立即拷贝进
//     SDL 纹理，item 随即归还 —— "在屏押 0 格"，比真机（阻塞 commit 押 1 格）
//     更宽松，语义兼容
//   - UI/overlay 是单 buffer 直绘：每帧从挂载 buffer 重新上传纹理
//   - MB32 tiled 在 PC 上退化为 planar NV12（pitch + uv_offset），
//     SDL_UpdateNVTexture 直接吃，DEFE 缩放 = RenderCopy 的 src/dst rect
//
// 线程模型：合成线程（本文件创建）身兼设备的 display thread 与 vblank——
// ~60Hz 消费队列、上传纹理、合成呈现；SDL 窗口/renderer/事件泵都在该线程
// （SDL 要求渲染与事件在创建线程）。
//
// PC 专属钩子（无头验证 / 脚本驱动）：
//   EPASS_SHOT=<path.bmp> + EPASS_SHOT_MS=<ms>  到点截图并触发正常退出
//   EPASS_AUTOKEY="<ms>:<key>[,...]"            到点注入按键(left/right/enter/esc/end)
//
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "utils/compat.h"
#include "config.h"

#include <SDL2/SDL.h>
#include <lvgl/lvgl.h> // LV_KEY_* 映射
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// key_sdl.c 提供：把 LVGL 键值注入 PC 按键队列
extern void key_sdl_inject(uint32_t lv_key);
// main.c 的运行标志：SDL_QUIT / EPASS_SHOT 走正常退出路径
extern int g_running;

#define SDL_FB_MAX 64
#define SDL_LAYER_MAX 4

// fb_id 注册表：allocate_buffer* 分配的 malloc 内存，fb_id = 下标(1 起)
typedef struct {
    bool used;
    uint8_t* vaddr;
    int width, height;
    int pitch;
    int uv_offset; // NV12 用；其余 0
    drm_warpper_layer_mode_t mode;
} sdl_fb_t;

typedef struct {
    bool inited;
    drm_warpper_layer_mode_t mode;
    int width, height;

    // 显示状态（合成线程读，API 线程写；对齐整型裸读写，撕裂语义同真机扫描竞争）
    bool enabled;
    int16_t x, y;
    uint8_t alpha;
    int src_w, src_h, dst_w, dst_h;

    // UI/overlay：单 buffer 直绘的挂载 buffer（每帧重新上传）
    uint8_t* mounted_vaddr;
    int mounted_pitch;

    // video：当前 FLIP 上屏的 fb（内容已拷入纹理，这里只留 id 供诊断）
    uint32_t curr_fb;

    SDL_Texture* tex;
    int tex_w, tex_h;
} sdl_layer_t;

typedef struct {
    uint32_t at_ms;
    uint32_t lv_key;
    bool fired;
} autokey_t;

static sdl_fb_t s_fbs[SDL_FB_MAX + 1];
static pthread_mutex_t s_fb_mtx = PTHREAD_MUTEX_INITIALIZER;
static sdl_layer_t s_layers[SDL_LAYER_MAX];
// 串行化"合成线程读挂载 buffer"与"free_buffer 释放该内存"：设备上 free 的是
// dumb buffer 而扫描走硬件不读 vaddr，PC 上合成线程要 memcpy，必须互斥
static pthread_mutex_t s_mount_mtx = PTHREAD_MUTEX_INITIALIZER;

static SDL_Window* s_win;
static SDL_Renderer* s_ren;
static pthread_t s_thread;
static atomic_int s_thread_running;
static atomic_int s_ready; // 0=初始化中 1=就绪 -1=失败

static autokey_t s_autokeys[16];
static int s_autokey_count;

// ---------------------------------------------------------------------------
// fb 注册表
// ---------------------------------------------------------------------------

static uint32_t fb_register(uint8_t* vaddr, int w, int h, int pitch, int uv_offset,
                            drm_warpper_layer_mode_t mode){
    pthread_mutex_lock(&s_fb_mtx);
    for(uint32_t i = 1; i <= SDL_FB_MAX; i++){
        if(!s_fbs[i].used){
            s_fbs[i].used = true;
            s_fbs[i].vaddr = vaddr;
            s_fbs[i].width = w;
            s_fbs[i].height = h;
            s_fbs[i].pitch = pitch;
            s_fbs[i].uv_offset = uv_offset;
            s_fbs[i].mode = mode;
            pthread_mutex_unlock(&s_fb_mtx);
            return i;
        }
    }
    pthread_mutex_unlock(&s_fb_mtx);
    log_error("[sdl] fb registry full");
    return 0;
}

static void fb_unregister(uint32_t fb_id){
    if(fb_id == 0 || fb_id > SDL_FB_MAX) return;
    pthread_mutex_lock(&s_fb_mtx);
    s_fbs[fb_id].used = false;
    pthread_mutex_unlock(&s_fb_mtx);
}

// ---------------------------------------------------------------------------
// 合成线程
// ---------------------------------------------------------------------------

static uint32_t layer_sdl_format(drm_warpper_layer_mode_t mode){
    switch(mode){
        case DRM_WARPPER_LAYER_MODE_RGB565:    return SDL_PIXELFORMAT_RGB565;
        case DRM_WARPPER_LAYER_MODE_MB32_NV12: return SDL_PIXELFORMAT_NV12;
        default:                               return SDL_PIXELFORMAT_ARGB8888;
    }
}

static void layer_ensure_texture(sdl_layer_t* l, int w, int h){
    if(l->tex && l->tex_w == w && l->tex_h == h) return;
    if(l->tex) SDL_DestroyTexture(l->tex);
    l->tex = SDL_CreateTexture(s_ren, layer_sdl_format(l->mode),
                               SDL_TEXTUREACCESS_STREAMING, w, h);
    l->tex_w = w;
    l->tex_h = h;
    SDL_SetTextureBlendMode(l->tex,
        l->mode == DRM_WARPPER_LAYER_MODE_ARGB8888 ? SDL_BLENDMODE_BLEND
                                                   : SDL_BLENDMODE_NONE);
}

// FLIP：帧内容立即拷进纹理（此后源 buffer 自由，item 可即刻归还）
static void layer_flip_upload(int layer_id, uint32_t fb_id){
    sdl_layer_t* l = &s_layers[layer_id];
    if(fb_id == 0 || fb_id > SDL_FB_MAX || !s_fbs[fb_id].used){
        log_error("[sdl] flip: bad fb_id %u", fb_id);
        return;
    }
    sdl_fb_t* fb = &s_fbs[fb_id];
    layer_ensure_texture(l, fb->width, fb->height);
    if(!l->tex) return;

    if(fb->mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        SDL_UpdateNVTexture(l->tex, NULL,
                            fb->vaddr, fb->pitch,
                            fb->vaddr + fb->uv_offset, fb->pitch);
    } else {
        SDL_UpdateTexture(l->tex, NULL, fb->vaddr, fb->pitch);
    }
    l->curr_fb = fb_id;
    l->enabled = true; // 惰性挂载语义：首帧启用 plane
}

static void compose_frame(drm_warpper_t* drm_warpper){
    // 1) 消费各层 display_queue
    for(int i = 0; i < SDL_LAYER_MAX; i++){
        layer_t* lq = &drm_warpper->layer[i];
        if(!lq->used) continue;

        drm_warpper_queue_item_t* item;
        while(spsc_bq_try_pop(&lq->display_queue, (void**)&item) == 0){
            switch(item->type){
            case DRM_WARPPER_ITEM_FLIP_FB:
                layer_flip_upload(i, item->fb_id);
                // 内容已拷贝，立即归还（on_heap 的便捷 item 不会是 FLIP）
                spsc_bq_push(&lq->free_queue, item);
                break;
            case DRM_WARPPER_ITEM_SET_COORD:
                s_layers[i].x = item->x;
                s_layers[i].y = item->y;
                if(item->on_heap) free(item);
                break;
            case DRM_WARPPER_ITEM_SET_ALPHA:
                s_layers[i].alpha = item->alpha;
                if(item->on_heap) free(item);
                break;
            }
        }
    }

    // 2) UI/overlay 单 buffer 直绘：每帧从挂载 buffer 上传
    pthread_mutex_lock(&s_mount_mtx);
    for(int i = 0; i < SDL_LAYER_MAX; i++){
        sdl_layer_t* l = &s_layers[i];
        if(!l->inited || !l->mounted_vaddr) continue;
        layer_ensure_texture(l, l->width, l->height);
        if(!l->tex) continue;
        SDL_UpdateTexture(l->tex, NULL, l->mounted_vaddr, l->mounted_pitch);
    }
    pthread_mutex_unlock(&s_mount_mtx);

    // 3) 自下而上合成（layer 0=video 最底，2=UI 最顶，同 DEBE）
    SDL_SetRenderDrawColor(s_ren, 0, 0, 0, 255);
    SDL_RenderClear(s_ren);
    for(int i = 0; i < SDL_LAYER_MAX; i++){
        sdl_layer_t* l = &s_layers[i];
        if(!l->inited || !l->enabled || !l->tex) continue;
        SDL_Rect src = { 0, 0, l->src_w > 0 ? l->src_w : l->tex_w,
                               l->src_h > 0 ? l->src_h : l->tex_h };
        SDL_Rect dst = { l->x, l->y, l->dst_w > 0 ? l->dst_w : src.w,
                                     l->dst_h > 0 ? l->dst_h : src.h };
        SDL_SetTextureAlphaMod(l->tex, l->alpha);
        SDL_RenderCopy(s_ren, l->tex, &src, &dst);
    }
    SDL_RenderPresent(s_ren);
}

static uint32_t map_sdl_key(SDL_Keycode k){
    switch(k){
        case SDLK_LEFT:     return LV_KEY_LEFT;
        case SDLK_RIGHT:    return LV_KEY_RIGHT;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return LV_KEY_ENTER;
        case SDLK_ESCAPE:   return LV_KEY_ESC;
        case SDLK_END:      return LV_KEY_END;
        default:            return 0;
    }
}

static uint32_t parse_key_name(const char* s){
    if(strcmp(s, "left") == 0)  return LV_KEY_LEFT;
    if(strcmp(s, "right") == 0) return LV_KEY_RIGHT;
    if(strcmp(s, "enter") == 0) return LV_KEY_ENTER;
    if(strcmp(s, "esc") == 0)   return LV_KEY_ESC;
    if(strcmp(s, "end") == 0)   return LV_KEY_END;
    return 0;
}

static void parse_autokeys(void){
    const char* env = getenv("EPASS_AUTOKEY");
    if(!env) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", env);
    char* save = NULL;
    for(char* tok = strtok_r(buf, ",", &save); tok && s_autokey_count < 16;
        tok = strtok_r(NULL, ",", &save)){
        char* colon = strchr(tok, ':');
        if(!colon) continue;
        *colon = '\0';
        uint32_t key = parse_key_name(colon + 1);
        if(!key) continue;
        s_autokeys[s_autokey_count].at_ms = (uint32_t)atoi(tok);
        s_autokeys[s_autokey_count].lv_key = key;
        s_autokey_count++;
    }
    log_info("[sdl] autokey: %d scheduled", s_autokey_count);
}

static void save_shot(const char* path){
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT,
                                                       24, SDL_PIXELFORMAT_RGB24);
    if(!surf ||
       SDL_RenderReadPixels(s_ren, NULL, SDL_PIXELFORMAT_RGB24, surf->pixels, surf->pitch) != 0 ||
       SDL_SaveBMP(surf, path) != 0){
        log_error("[sdl] EPASS_SHOT failed: %s", SDL_GetError());
    } else {
        log_info("[sdl] EPASS_SHOT saved: %s", path);
    }
    if(surf) SDL_FreeSurface(surf);
}

static void* sdl_display_thread(void* arg){
    drm_warpper_t* drm_warpper = (drm_warpper_t*)arg;

    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        log_error("[sdl] SDL_Init failed: %s", SDL_GetError());
        atomic_store(&s_ready, -1);
        return NULL;
    }
    const char* wx = getenv("SIM_WIN_X");
    const char* wy = getenv("SIM_WIN_Y");
    char title[64];
    snprintf(title, sizeof(title), "EPass PC %dx%d", SCREEN_WIDTH, SCREEN_HEIGHT);
    s_win = SDL_CreateWindow(title,
                             wx ? atoi(wx) : (int)SDL_WINDOWPOS_UNDEFINED,
                             wy ? atoi(wy) : (int)SDL_WINDOWPOS_UNDEFINED,
                             SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if(!s_win){
        log_error("[sdl] SDL_CreateWindow failed: %s", SDL_GetError());
        atomic_store(&s_ready, -1);
        return NULL;
    }
    // 软渲染：dummy 驱动可跑、RenderReadPixels 可靠；两档分辨率软合成绰绰有余
    s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_SOFTWARE);
    if(!s_ren){
        log_error("[sdl] SDL_CreateRenderer failed: %s", SDL_GetError());
        atomic_store(&s_ready, -1);
        return NULL;
    }

    const char* shot_path = getenv("EPASS_SHOT");
    uint32_t shot_ms = 3000;
    if(getenv("EPASS_SHOT_MS")) shot_ms = (uint32_t)atoi(getenv("EPASS_SHOT_MS"));
    parse_autokeys();

    atomic_store(&s_ready, 1);
    log_info("==> [sdl] display thread started (%dx%d)", SCREEN_WIDTH, SCREEN_HEIGHT);

    while(atomic_load(&s_thread_running)){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type == SDL_QUIT){
                g_running = 0;
            } else if(e.type == SDL_KEYDOWN){
                uint32_t k = map_sdl_key(e.key.keysym.sym);
                if(k) key_sdl_inject(k);
            }
        }

        uint32_t now = SDL_GetTicks();
        for(int i = 0; i < s_autokey_count; i++){
            if(!s_autokeys[i].fired && now >= s_autokeys[i].at_ms){
                s_autokeys[i].fired = true;
                key_sdl_inject(s_autokeys[i].lv_key);
            }
        }

        compose_frame(drm_warpper);

        if(shot_path && now >= shot_ms){
            save_shot(shot_path);
            shot_path = NULL;
            g_running = 0;
        }

        SDL_Delay(16);
    }

    for(int i = 0; i < SDL_LAYER_MAX; i++){
        if(s_layers[i].tex) SDL_DestroyTexture(s_layers[i].tex);
        s_layers[i].tex = NULL;
    }
    SDL_DestroyRenderer(s_ren);
    SDL_DestroyWindow(s_win);
    SDL_Quit();
    log_info("==> [sdl] display thread ended");
    return NULL;
}

// ---------------------------------------------------------------------------
// drm_warpper API（PC 实现）
// ---------------------------------------------------------------------------

int drm_warpper_init(drm_warpper_t *drm_warpper){
    memset(drm_warpper, 0, sizeof(*drm_warpper));
    memset(s_layers, 0, sizeof(s_layers));
    memset(s_fbs, 0, sizeof(s_fbs));

    // PC 数据目录（settings/logs 等落这里，免 root 文件系统路径）
    mkdir(EPASS_PC_DATA_DIR, 0755);

    atomic_store(&s_ready, 0);
    atomic_store(&s_thread_running, 1);
    if(pthread_create(&s_thread, NULL, sdl_display_thread, drm_warpper) != 0){
        log_error("[sdl] display thread create failed");
        return -1;
    }
    // 等 SDL 窗口就绪（渲染资源都在合成线程创建）
    while(atomic_load(&s_ready) == 0){
        SDL_Delay(5);
    }
    if(atomic_load(&s_ready) < 0){
        atomic_store(&s_thread_running, 0);
        pthread_join(s_thread, NULL);
        return -1;
    }
    log_info("==> [sdl] drm_warpper (SDL backend) initialized");
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    atomic_store(&s_thread_running, 0);
    pthread_join(s_thread, NULL);
    for(int i = 0; i < SDL_LAYER_MAX; i++){
        if(drm_warpper->layer[i].used){
            drm_warpper_destroy_layer(drm_warpper, i);
        }
    }
    return 0;
}

int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode){
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX) return -1;
    layer_t* lq = &drm_warpper->layer[layer_id];
    if(spsc_bq_init(&lq->display_queue, 16) != 0) return -1;
    if(spsc_bq_init(&lq->free_queue, 20) != 0) return -1;
    lq->used = true;
    lq->mode = mode;
    lq->width = width;
    lq->height = height;

    sdl_layer_t* l = &s_layers[layer_id];
    memset(l, 0, sizeof(*l));
    l->inited = true;
    l->mode = mode;
    l->width = width;
    l->height = height;
    l->alpha = 255;
    return 0;
}

int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX) return -1;
    layer_t* lq = &drm_warpper->layer[layer_id];
    if(!lq->used) return 0;
    spsc_bq_destroy(&lq->display_queue);
    spsc_bq_destroy(&lq->free_queue);
    lq->used = false;
    s_layers[layer_id].inited = false;
    return 0;
}

static int mode_bpp(drm_warpper_layer_mode_t mode){
    switch(mode){
        case DRM_WARPPER_LAYER_MODE_RGB565: return 2;
        case DRM_WARPPER_LAYER_MODE_ARGB8888: return 4;
        default: return 1; // NV12：按 1bpp 的 pitch，高度另乘 3/2
    }
}

int drm_warpper_allocate_buffer_sized(drm_warpper_t *drm_warpper,int layer_id,int width,int height,buffer_object_t *buf){
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX || !s_layers[layer_id].inited) return -1;
    drm_warpper_layer_mode_t mode = s_layers[layer_id].mode;
    int bpp = mode_bpp(mode);
    int pitch = width * bpp;
    int uv_offset = 0;
    size_t size;

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        // PC 上退化为 planar NV12：Y 满幅 + UV 半高
        uv_offset = pitch * height;
        size = (size_t)pitch * height * 3 / 2;
    } else {
        size = (size_t)pitch * height;
    }

    memset(buf, 0, sizeof(*buf));
    buf->vaddr = calloc(1, size);
    if(!buf->vaddr) return -1;
    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        // 黑帧：UV=128
        memset(buf->vaddr + uv_offset, 128, size - (size_t)uv_offset);
    }
    buf->width = (uint32_t)width;
    buf->height = (uint32_t)height;
    buf->pitch = (uint32_t)pitch;
    buf->size = (uint32_t)size;
    buf->fb_id = fb_register(buf->vaddr, width, height, pitch, uv_offset, mode);
    if(buf->fb_id == 0){
        free(buf->vaddr);
        buf->vaddr = NULL;
        return -1;
    }
    buf->handle = buf->fb_id;
    return 0;
}

int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX || !s_layers[layer_id].inited) return -1;
    return drm_warpper_allocate_buffer_sized(drm_warpper, layer_id,
                                             s_layers[layer_id].width,
                                             s_layers[layer_id].height, buf);
}

int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    (void)drm_warpper;
    (void)layer_id;
    fb_unregister(buf->fb_id);
    // 该 buffer 可能正被合成线程当挂载层上传：先摘除挂载再释放（互斥保证不撞车）
    pthread_mutex_lock(&s_mount_mtx);
    for(int i = 0; i < SDL_LAYER_MAX; i++){
        if(s_layers[i].mounted_vaddr == buf->vaddr){
            s_layers[i].mounted_vaddr = NULL;
            s_layers[i].enabled = false;
        }
    }
    free(buf->vaddr);
    pthread_mutex_unlock(&s_mount_mtx);
    buf->vaddr = NULL;
    buf->fb_id = 0;
    return 0;
}

int drm_warpper_import_dmabuf_fb(drm_warpper_t *drm_warpper,int dmabuf_fd,int width,int height,int pitch,int uv_offset,uint32_t *fb_id,uint32_t *gem_handle){
    (void)drm_warpper; (void)dmabuf_fd; (void)width; (void)height;
    (void)pitch; (void)uv_offset; (void)fb_id; (void)gem_handle;
    log_error("[sdl] import_dmabuf_fb not supported on PC (use allocate_buffer_sized)");
    return -1;
}

int drm_warpper_rm_fb(drm_warpper_t *drm_warpper,uint32_t fb_id,uint32_t gem_handle){
    (void)drm_warpper; (void)gem_handle;
    fb_unregister(fb_id);
    return 0;
}

int drm_warpper_mount_layer_rect(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf,int src_w,int src_h,int dst_w,int dst_h){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX) return -1;
    sdl_layer_t* l = &s_layers[layer_id];
    l->mounted_vaddr = buf->vaddr;
    l->mounted_pitch = (int)buf->pitch;
    l->width = (int)buf->width;
    l->height = (int)buf->height;
    l->x = (int16_t)x;
    l->y = (int16_t)y;
    l->src_w = src_w;
    l->src_h = src_h;
    l->dst_w = dst_w;
    l->dst_h = dst_h;
    l->enabled = true;
    return 0;
}

int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    return drm_warpper_mount_layer_rect(drm_warpper, layer_id, x, y, buf,
                                        (int)buf->width, (int)buf->height,
                                        (int)buf->width, (int)buf->height);
}

int drm_warpper_set_layer_geometry(drm_warpper_t *drm_warpper,int layer_id,int x,int y,int src_w,int src_h,int dst_w,int dst_h){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX) return -1;
    sdl_layer_t* l = &s_layers[layer_id];
    l->x = (int16_t)x;
    l->y = (int16_t)y;
    l->src_w = src_w;
    l->src_h = src_h;
    l->dst_w = dst_w;
    l->dst_h = dst_h;
    // 惰性挂载：plane 由首个 FLIP 启用（layer_flip_upload 置 enabled）
    return 0;
}

int drm_warpper_disable_layer_sync(drm_warpper_t *drm_warpper,int layer_id){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= SDL_LAYER_MAX) return -1;
    s_layers[layer_id].enabled = false;
    s_layers[layer_id].curr_fb = 0;
    return 0;
}

int drm_warpper_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item){
    return spsc_bq_push(&drm_warpper->layer[layer_id].display_queue, item);
}

int drm_warpper_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    return spsc_bq_pop(&drm_warpper->layer[layer_id].free_queue, (void**)out_item);
}

int drm_warpper_try_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    return spsc_bq_try_pop(&drm_warpper->layer[layer_id].free_queue, (void**)out_item);
}

int drm_warpper_set_layer_coord(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    drm_warpper_queue_item_t* item = malloc(sizeof(*item));
    if(!item) return -1;
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_SET_COORD;
    item->x = (int16_t)x;
    item->y = (int16_t)y;
    item->on_heap = true;
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha){
    drm_warpper_queue_item_t* item = malloc(sizeof(*item));
    if(!item) return -1;
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_SET_ALPHA;
    item->alpha = (uint8_t)alpha;
    item->on_heap = true;
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}
