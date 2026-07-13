//
// Overlay 仿真入口 —— 真实的 overlay 代码（opinfo 元素引擎 / transitions /
// worker / prts_timer / layer_animation 全链路）跑在 SDL 合成窗口上。
//
// 图层模型（对应设备 DEBE 自下而上）：
//   底层 video  = 编译期宏 SIM_VIDEO_IMG 指定的固定图片（替代 Cedar 解码）
//   上层 overlay = mock drm_warpper 的 ARGB buffer，按层坐标/alpha 混叠
//
// 按键：1..5 切干员预设   0/ESC 仅 abort（看滑出）
//       f/m/w fade/move/swipe 过渡   q 退出
// 环境变量（无头验证用）：
//   SIM_PRESET=<1..5>  启动即显示该预设
//   SIM_SHOT=<path.bmp> SIM_SHOT_MS=<ms>  到点截图后退出
//   SIM_WIN_X/SIM_WIN_Y 窗口位置
//
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "config.h"
#include "ui_metrics.h"
#include "driver/drm_warpper.h"
#include "render/layer_animation.h"
#include "overlay/overlay.h"
#include "overlay/opinfo.h"
#include "overlay/transitions.h"
#include "utils/timer.h"
#include "utils/cacheassets.h"
#include "utils/respath.h"
#include "utils/stb_image.h"
#include "utils/log.h"
#include "ui/font_registry.h"

#include "overlay_sim_presets.h"

// sim_drm_warpper.c 暴露的层状态读取
void sim_drm_layer_state(int layer_id, uint8_t** vaddr, int* x, int* y, int* alpha);

static drm_warpper_t g_drm_warpper;
static layer_animation_t g_layer_animation;
static overlay_t g_overlay;
static prts_timer_t g_prts_timer;
static cacheassets_t g_cacheassets_local; // cacheassets_get_asset_from_global 用全局单例
static olopinfo_params_t g_params;
static int g_params_valid;

#ifndef SIM_VIDEO_IMG
#define SIM_VIDEO_IMG ""
#endif

static uint32_t tick_cb(void){ return SDL_GetTicks(); }

static void show_preset(int idx){
    log_info("[sim] ===== preset %d: %s =====", idx + 1, overlay_sim_preset_name(idx));
    overlay_abort(&g_overlay);
    if(g_params_valid){
        overlay_opinfo_free_elements(&g_params);
        g_params_valid = 0;
    }
    if(overlay_sim_preset_build(idx, &g_params) != 0){
        log_error("[sim] preset build failed");
        return;
    }
    g_params_valid = 1;
    overlay_opinfo_load_image(&g_params);
    overlay_opinfo_show_elements(&g_overlay, &g_params);
}

// ---------------------------------------------------------------------------
// transitions（静态生命周期的 params/callback，middle/end 只打日志）
// ---------------------------------------------------------------------------
static oltr_params_t g_tr_params;
static oltr_callback_t g_tr_cb;

static void tr_middle_cb(void* ud, bool is_last){ (void)ud; (void)is_last; log_info("[sim] transition middle"); }
static void tr_end_cb(void* ud, bool is_last){ (void)ud; (void)is_last; log_info("[sim] transition end"); }

static void show_transition(transition_type_t type){
    overlay_abort(&g_overlay);
    overlay_transition_free_image(&g_tr_params);

    memset(&g_tr_params, 0, sizeof(g_tr_params));
    g_tr_params.type = type;
    g_tr_params.duration = 600 * 1000;
    g_tr_params.background_color = 0xFF1B3A5Bu;
    g_tr_params.src_upscale = 1;
    snprintf(g_tr_params.image_path, sizeof(g_tr_params.image_path), "%s", respath("prts_64_inv.png"));
    overlay_transition_load_image(&g_tr_params);

    memset(&g_tr_cb, 0, sizeof(g_tr_cb));
    g_tr_cb.middle_cb = tr_middle_cb;
    g_tr_cb.end_cb = tr_end_cb;

    switch(type){
        case TRANSITION_TYPE_FADE:  log_info("[sim] transition: fade");  overlay_transition_fade(&g_overlay, &g_tr_cb, &g_tr_params);  break;
        case TRANSITION_TYPE_MOVE:  log_info("[sim] transition: move");  overlay_transition_move(&g_overlay, &g_tr_cb, &g_tr_params);  break;
        case TRANSITION_TYPE_SWIPE: log_info("[sim] transition: swipe"); overlay_transition_swipe(&g_overlay, &g_tr_cb, &g_tr_params); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// video 底图
// ---------------------------------------------------------------------------
static SDL_Texture* load_video_texture(SDL_Renderer* r){
    int w, h, c;
    uint8_t* pix = NULL;
    if(SIM_VIDEO_IMG[0] != '\0'){
        pix = stbi_load(SIM_VIDEO_IMG, &w, &h, &c, 4);
    }
    if(!pix){
        log_warn("[sim] SIM_VIDEO_IMG 加载失败(%s)，用灰格底", SIM_VIDEO_IMG);
        w = SCREEN_WIDTH; h = SCREEN_HEIGHT;
        pix = malloc((size_t)w * h * 4);
        for(int y = 0; y < h; y++){
            for(int x = 0; x < w; x++){
                uint8_t v = (((x / 32) + (y / 32)) & 1) ? 0x30 : 0x50;
                uint32_t* px = (uint32_t*)pix + y * w + x;
                *px = 0xFF000000u | (v << 16) | (v << 8) | v;
            }
        }
    }
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(tex, NULL, pix, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    stbi_image_free(pix); // malloc 的灰格同样可用 free 释放（stbi_image_free 即 free）
    return tex;
}

int main(int argc, char* argv[]){
    (void)argc; (void)argv;

    log_info("==> EPass Overlay Simulator (%dx%d, UI_SCALE=%d)",
             SCREEN_WIDTH, SCREEN_HEIGHT, UI_SCALE);

    respath_init();

    lv_init();
    lv_tick_set_cb(tick_cb);

    prts_timer_init(&g_prts_timer);

    drm_warpper_init(&g_drm_warpper);
    drm_warpper_init_layer(&g_drm_warpper, DRM_WARPPER_LAYER_OVERLAY,
                           OVERLAY_WIDTH, OVERLAY_HEIGHT, DRM_WARPPER_LAYER_MODE_ARGB8888);

    // cacheassets：与设备 main.c 同一组资产
    uint8_t* cache_buf = malloc(CACHED_ASSETS_MAX_SIZE);
    if(!cache_buf){
        log_error("[sim] cacheassets buffer alloc failed");
        return 1;
    }
    cacheassets_init(&g_cacheassets_local, cache_buf, CACHED_ASSETS_MAX_SIZE);
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_AK_BAR, (char*)respath(CACHED_ASSETS_FILE_AK_BAR));
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_BTM_LEFT_BAR, (char*)respath(CACHED_ASSETS_FILE_BTM_LEFT_BAR));
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_TOP_LEFT_RECT, (char*)respath(CACHED_ASSETS_FILE_TOP_LEFT_RECT));
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_TOP_LEFT_RHODES, (char*)respath(CACHED_ASSETS_FILE_TOP_LEFT_RHODES));
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_TOP_RIGHT_BAR, (char*)respath(CACHED_ASSETS_FILE_TOP_RIGHT_BAR));
    cacheassets_put_asset(&g_cacheassets_local, CACHE_ASSETS_TOP_RIGHT_ARROW, (char*)respath(CACHED_ASSETS_FILE_TOP_RIGHT_ARROW));
    log_info("[sim] cached assets: %d bytes", g_cacheassets_local.curr_size);

    if(font_registry_init() != 0){
        log_error("[sim] font_registry_init failed");
        return 1;
    }

    layer_animation_init(&g_layer_animation, &g_drm_warpper);
    overlay_init(&g_overlay, &g_drm_warpper, &g_layer_animation);

    // ---------------- SDL ----------------
    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        log_error("[sim] SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const char* wx = getenv("SIM_WIN_X");
    const char* wy = getenv("SIM_WIN_Y");
    int pos_x = wx ? atoi(wx) : (int)SDL_WINDOWPOS_UNDEFINED;
    int pos_y = wy ? atoi(wy) : (int)SDL_WINDOWPOS_UNDEFINED;
    char title[80];
    snprintf(title, sizeof(title), "EPass Overlay Simulator %dx%d", SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_Window* win = SDL_CreateWindow(title, pos_x, pos_y, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    // 软渲染：dummy 驱动可跑、RenderReadPixels 可靠，这个分辨率绰绰有余
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

    SDL_Texture* video_tex = load_video_texture(ren);
    SDL_Texture* overlay_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 OVERLAY_WIDTH, OVERLAY_HEIGHT);
    SDL_SetTextureBlendMode(overlay_tex, SDL_BLENDMODE_BLEND);

    // ---------------- 无头验证参数 ----------------
    const char* shot_path = getenv("SIM_SHOT");
    uint32_t shot_ms = 1500;
    const char* shot_ms_s = getenv("SIM_SHOT_MS");
    if(shot_ms_s) shot_ms = (uint32_t)atoi(shot_ms_s);

    const char* preset_s = getenv("SIM_PRESET");
    if(preset_s){
        int idx = atoi(preset_s) - 1;
        if(idx >= 0 && idx < OVERLAY_SIM_PRESET_COUNT) show_preset(idx);
    }

    log_info("[sim] keys: 1..%d preset | 0/ESC abort | f/m/w transition | q quit",
             OVERLAY_SIM_PRESET_COUNT);

    // ---------------- 合成主循环 ----------------
    bool running = true;
    while(running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type == SDL_QUIT){
                running = false;
            }
            else if(e.type == SDL_KEYDOWN){
                SDL_Keycode k = e.key.keysym.sym;
                if(k >= SDLK_1 && k < SDLK_1 + OVERLAY_SIM_PRESET_COUNT){
                    show_preset(k - SDLK_1);
                }
                else if(k == SDLK_0 || k == SDLK_ESCAPE){
                    log_info("[sim] overlay_abort");
                    overlay_abort(&g_overlay);
                }
                else if(k == SDLK_f) show_transition(TRANSITION_TYPE_FADE);
                else if(k == SDLK_m) show_transition(TRANSITION_TYPE_MOVE);
                else if(k == SDLK_w) show_transition(TRANSITION_TYPE_SWIPE);
                else if(k == SDLK_q) running = false;
            }
        }

        lv_timer_handler();

        // 合成：video 铺满 → overlay 按层坐标/alpha 混叠（等价 DEBE）
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, video_tex, NULL, NULL);

        uint8_t* ol_vaddr;
        int ol_x, ol_y, ol_alpha;
        sim_drm_layer_state(DRM_WARPPER_LAYER_OVERLAY, &ol_vaddr, &ol_x, &ol_y, &ol_alpha);
        if(ol_vaddr){
            SDL_UpdateTexture(overlay_tex, NULL, ol_vaddr, OVERLAY_WIDTH * 4);
            SDL_SetTextureAlphaMod(overlay_tex, (uint8_t)ol_alpha);
            SDL_Rect dst = { ol_x, ol_y, OVERLAY_WIDTH, OVERLAY_HEIGHT };
            SDL_RenderCopy(ren, overlay_tex, NULL, &dst);
        }
        SDL_RenderPresent(ren);

        if(shot_path && SDL_GetTicks() >= shot_ms){
            SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT,
                                                               24, SDL_PIXELFORMAT_RGB24);
            if(!surf ||
               SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGB24, surf->pixels, surf->pitch) != 0 ||
               SDL_SaveBMP(surf, shot_path) != 0){
                log_error("[sim] SIM_SHOT failed: %s", SDL_GetError());
                return 1;
            }
            log_info("[sim] SIM_SHOT saved: %s", shot_path);
            break;
        }

        SDL_Delay(16);
    }

    // 收尾：让 worker 把资源清干净（也顺带验证 abort 路径不卡死）
    overlay_abort(&g_overlay);
    if(g_params_valid){
        overlay_opinfo_free_elements(&g_params);
    }
    overlay_destroy(&g_overlay);
    return 0;
}
