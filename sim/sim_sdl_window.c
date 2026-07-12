#include "sim_sdl_window.h"
#include "config.h"
#include "utils/log.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

// 单显示器模拟器，单例状态足够。
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    SDL_Texture  *bg;        // 可选背景 (SIM_BG), 模拟 UI 层下方的立绘视频层
    uint8_t      *buf;       // LVGL DIRECT 单缓冲 (RGB565)
    int32_t       w, h;
    int           plane_y;   // viewport Y 偏移
} sim_window_t;

static sim_window_t  s_win;
static lv_indev_t   *s_indev;
static sim_key_cb_t  s_key_cb;

// 输入走事件模式 + 字节队列 (同 vendored lv_sdl_keyboard)：每次按下手动
// lv_indev_read 两次(按下+松开)，避免轮询模式下快速点按因落在同一读周期内而丢键。
static uint8_t s_kbuf[32];
static size_t  s_klen;
static bool    s_dummy_read;

// ---------------------------------------------------------------------------
// 输入
// ---------------------------------------------------------------------------

// SDL 键码 → LVGL 键，对齐 vendored lv_sdl_keyboard 的映射 (仅取导航/编辑所需)。
static uint32_t map_key(SDL_Keycode k)
{
    switch (k) {
        case SDLK_RIGHT:    return LV_KEY_RIGHT;
        case SDLK_LEFT:     return LV_KEY_LEFT;
        case SDLK_UP:       return LV_KEY_UP;
        case SDLK_DOWN:     return LV_KEY_DOWN;
        case SDLK_ESCAPE:   return LV_KEY_ESC;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return LV_KEY_ENTER;
        case SDLK_TAB:      return LV_KEY_NEXT;
        case SDLK_END:      return LV_KEY_END;
        default:            return 0;
    }
}

static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static uint32_t last_key;
    if (s_dummy_read) {                 // 队列里每个键先发 PRESSED，再补一次 RELEASED
        s_dummy_read = false;           // (松开也须回传 last_key，否则被当成松开了别的键)
        data->key    = last_key;
        data->state  = LV_INDEV_STATE_RELEASED;
    } else if (s_klen > 0) {
        s_dummy_read = true;
        data->state  = LV_INDEV_STATE_PRESSED;
        data->key    = last_key = s_kbuf[0];
        memmove(s_kbuf, s_kbuf + 1, --s_klen);
    }
}

static void event_timer_cb(lv_timer_t *t)
{
    (void)t;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_KEYDOWN: {
                uint32_t k = map_key(e.key.keysym.sym);
                if (!k) break;
                if (s_key_cb) s_key_cb(k);  // 导航(切屏)：直调状态机，同设备 evdev.input_cb
                // 同一键也入队喂 group(屏内焦点/编辑)，再手动 read 出按下+松开
                if (s_klen < sizeof(s_kbuf)) s_kbuf[s_klen++] = (uint8_t)k;
                lv_indev_read(s_indev);
                lv_indev_read(s_indev);
                break;
            }
            case SDL_QUIT:
                exit(0);
            default:
                break;
        }
    }
}

void sim_window_set_key_cb(sim_key_cb_t cb) { s_key_cb = cb; }
lv_indev_t *sim_window_indev(void)          { return s_indev; }

// ---------------------------------------------------------------------------
// 截图 (无人值守出图)
//   SIM_SHOT=<path.bmp>   渲染稳定后存 BMP 并 exit(0)
//   SIM_SHOT_MS=<延时ms>  等待时长, 默认 1500 (盖过切屏幕帘动画)
// 与 SIM_SCREEN 组合即可逐屏批量截图 (见 epass_applications quick_start 的截图脚本)。
// ---------------------------------------------------------------------------

static void render_frame(void);

static void shot_timer_cb(lv_timer_t *t)
{
    const char *path = lv_timer_get_user_data(t);
    // sysmon 性能标签挂 sys 层, 不该进图; 藏掉后重新渲染一帧再读
    lv_obj_add_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);
    // 读后台缓冲须在 present 之前: 重画一遍再读, 不 present
    render_frame();
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, s_win.w, s_win.h, 24,
                                                       SDL_PIXELFORMAT_RGB24);
    if (!surf ||
        SDL_RenderReadPixels(s_win.renderer, NULL, SDL_PIXELFORMAT_RGB24,
                             surf->pixels, surf->pitch) != 0 ||
        SDL_SaveBMP(surf, path) != 0) {
        log_error("SIM_SHOT failed: %s", SDL_GetError());
        exit(1);
    }
    log_info("SIM_SHOT saved: %s", path);
    exit(0);
}

// ---------------------------------------------------------------------------
// 呈现 (viewport)
// ---------------------------------------------------------------------------

void sim_window_set_plane_y(int y) { s_win.plane_y = y; }
int  sim_window_get_plane_y(void)  { return s_win.plane_y; }

// 露出处填 SIM_BG 或黑 (替设备 UI 层下方的立绘背景层)
static void render_frame(void)
{
    SDL_RenderClear(s_win.renderer);
    if (s_win.bg) SDL_RenderCopy(s_win.renderer, s_win.bg, NULL, NULL);
    SDL_Rect dst = { 0, s_win.plane_y, s_win.w, s_win.h };
    SDL_RenderCopy(s_win.renderer, s_win.texture, NULL, &dst);
}

void sim_window_present(void)
{
    render_frame();
    SDL_RenderPresent(s_win.renderer);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    // DIRECT 模式: 整帧渲染在缓冲里，最后一块时 px_map 指向缓冲基址。
    if (lv_display_flush_is_last(disp)) {
        SDL_UpdateTexture(s_win.texture, NULL, px_map, s_win.w * 2);
        sim_window_present();
    }
    lv_display_flush_ready(disp);
}

// ---------------------------------------------------------------------------
// 建窗
// ---------------------------------------------------------------------------

lv_display_t *sim_window_create(int32_t hor_res, int32_t ver_res)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return NULL;
    }
    SDL_StartTextInput();

    s_win.w       = hor_res;
    s_win.h       = ver_res;
    s_win.plane_y = SCREEN_HEIGHT;

    // 窗口位置可由 SIM_WIN_X/SIM_WIN_Y 指定 (便于脚本把两档并排摆放)，否则交给 WM。
    const char *wx = getenv("SIM_WIN_X");
    const char *wy = getenv("SIM_WIN_Y");
    int pos_x = wx ? atoi(wx) : (int)SDL_WINDOWPOS_UNDEFINED;
    int pos_y = wy ? atoi(wy) : (int)SDL_WINDOWPOS_UNDEFINED;

    char title[64];
    snprintf(title, sizeof(title), "EPass UI Simulator %dx%d", hor_res, ver_res);
    s_win.window = SDL_CreateWindow(title, pos_x, pos_y, hor_res, ver_res, 0);
    if (!s_win.window) {
        log_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return NULL;
    }
    s_win.renderer = SDL_CreateRenderer(s_win.window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(s_win.renderer, 0, 0, 0, 255);

    s_win.texture = SDL_CreateTexture(s_win.renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING, hor_res, ver_res);
    SDL_SetTextureBlendMode(s_win.texture, SDL_BLENDMODE_NONE);

    size_t buf_sz = (size_t)hor_res * ver_res * 2;
    s_win.buf = malloc(buf_sz);
    if (!s_win.buf) {
        log_error("draw buffer alloc failed");
        return NULL;
    }

    lv_display_t *disp = lv_display_create(hor_res, ver_res);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_win.buf, NULL, buf_sz, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);

    // 喂 group 做屏内焦点/编辑 (导航另走 key 回调)。
    // 必须是 ENCODER 而非 KEYPAD：设备侧 key_enc_evdev 就是 ENCODER，左/右键
    // (LV_KEY_LEFT/RIGHT) 在 encoder 处理里才会移焦点；keypad 段只认 NEXT/PREV(Tab)。
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(s_indev, encoder_read_cb);
    lv_indev_set_mode(s_indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_display(s_indev, disp);

    lv_timer_create(event_timer_cb, 5, NULL);  // SDL 事件泵

    const char *bg = getenv("SIM_BG");  // BMP, 铺满窗口当立绘背景
    if (bg) {
        SDL_Surface *s = SDL_LoadBMP(bg);
        if (s) {
            s_win.bg = SDL_CreateTextureFromSurface(s_win.renderer, s);
            SDL_FreeSurface(s);
        } else {
            log_error("SIM_BG load failed: %s", SDL_GetError());
        }
    }

    const char *shot = getenv("SIM_SHOT");
    if (shot) {
        const char *ms = getenv("SIM_SHOT_MS");
        lv_timer_t *t = lv_timer_create(shot_timer_cb, ms ? (uint32_t)atoi(ms) : 1500, (void *)shot);
        lv_timer_set_repeat_count(t, 1);
    }

    return disp;
}
