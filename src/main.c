#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#include "config.h"
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "render/mediaplayer.h"
#include "render/lvgl_drm_warp.h"
#include "overlay/overlay.h"
#include "overlay/transitions.h"
#include "overlay/opinfo.h"
#include "utils/timer.h"
#include "render/layer_animation.h"
#include "utils/settings.h"
#include "utils/timer.h"
#include "utils/cacheassets.h"
#include "ui/actions_warning.h"

/* global variables */
drm_warpper_t g_drm_warpper;
mediaplayer_t g_mediaplayer;
lvgl_drm_warp_t g_lvgl_drm_warp;
prts_timer_t g_prts_timer;
layer_animation_t g_layer_animation;
settings_t g_settings;
overlay_t g_overlay;
cacheassets_t g_cacheassets;

buffer_object_t g_video_buf;

int g_running = 1;
int g_exitcode = 0;

void signal_handler(int sig)
{
    log_info("received signal %d, shutting down", sig);
    g_running = 0;
    g_exitcode = 0;
}

void mount_video_layer_callback(void *userdata,bool is_last){
    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0, &g_video_buf);
    drm_warpper_set_layer_coord(&g_drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0,0);
}


int main(int argc, char *argv[]){
    if(argc == 2){
        if(strcmp(argv[1], "version") == 0){
            printf("EPASS_NEO_GIT_VERSION: %s\n", EPASS_GIT_VERSION);
            printf("COMPILE_TIME: %s\n", COMPILE_TIME);
            return 0;
        }
        else if(strcmp(argv[1], "aux") == 0){
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("EPASS_NEO_GIT_VERSION: %s\n", EPASS_GIT_VERSION);
    printf("COMPILE_TIME: %s\n", COMPILE_TIME);

    fputs(APP_BARNER, stderr);
    log_info("==========> Starting EPass DRM APP!");

    usleep(3000000);

    if(drm_warpper_init(&g_drm_warpper) != 0){
        log_error("failed to initialize DRM warpper");
        return -1;
    }

    // ============ PRTS 定时器初始化 ===============
    prts_timer_init(&g_prts_timer);

    // ============ 设置 初始化 ===============
    settings_init(&g_settings);

    // ============ 图层动画 初始化 ===============
    layer_animation_init(&g_layer_animation, &g_drm_warpper);

    // ============ Mediaplayer 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_VIDEO, 
        VIDEO_WIDTH, 
        VIDEO_HEIGHT, 
    DRM_WARPPER_LAYER_MODE_MB32_NV12);

    // FIXME：
    // 用来跑modeset的buffer，实际上是不用的，这一片内存你也可以拿去干别的
    // 期待有能人帮优化掉这个allocate。
    drm_warpper_allocate_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, &g_video_buf);
    // drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0, &video_buf);

    /* initialize mediaplayer */
    if (mediaplayer_init(&g_mediaplayer, &g_drm_warpper) != 0) {
        log_error("failed to initialize mediaplayer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }

    mediaplayer_set_video(&g_mediaplayer, "/assets/MS/loop.mp4");
    mediaplayer_start(&g_mediaplayer);

    // ============ 缓冲素材 初始化 ===============
    // 没错，我就是要用video层的buffer来缓存素材
    cacheassets_init(&g_cacheassets, g_video_buf.vaddr, CACHED_ASSETS_MAX_SIZE);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_AK_BAR, CACHED_ASSETS_ASSET_PATH_AK_BAR);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_BTM_LEFT_BAR, CACHED_ASSETS_ASSET_PATH_BTM_LEFT_BAR);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_LEFT_RECT, CACHED_ASSETS_ASSET_PATH_TOP_LEFT_RECT);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_LEFT_RHODES, CACHED_ASSETS_ASSET_PATH_TOP_LEFT_RHODES);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_RIGHT_BAR, CACHED_ASSETS_ASSET_PATH_TOP_RIGHT_BAR);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_RIGHT_ARROW, CACHED_ASSETS_ASSET_PATH_TOP_RIGHT_ARROW);

    log_info("Cached assets: %d", g_cacheassets.curr_size);

    // ============ OVERLAY 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_OVERLAY, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_ARGB8888
    );

    overlay_init(&g_overlay, &g_drm_warpper, &g_layer_animation);


    // ============ LVGL 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_UI, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_RGB565
    );

    lvgl_drm_warp_init(&g_lvgl_drm_warp, &g_drm_warpper,&g_layer_animation);
    drm_warpper_set_layer_coord(&g_drm_warpper, DRM_WARPPER_LAYER_UI, 0, SCREEN_HEIGHT);


    oltr_params_t fade_params;
    fade_params.duration = 500000;
    strcpy(fade_params.image_path, "/root/u_boot_logo.png");
    fade_params.background_color = 0xFF000000;
    overlay_transition_load_image(&fade_params);
    // overlay_transition_fade(&g_overlay,mount_video_layer_callback,NULL,&fade_params);
    // overlay_transition_move(&g_overlay,mount_video_layer_callback,NULL,&fade_params);
    overlay_transition_swipe(&g_overlay,mount_video_layer_callback,NULL,&fade_params);


    usleep(3 * 1000 * 1000);

    olopinfo_params_t opinfo_params;
    opinfo_params.type = OPINFO_TYPE_ARKNIGHTS;
    opinfo_params.fade_duration = 500000;
    opinfo_params.color = 0x00ff0000;
    strcpy(opinfo_params.operator_name, "SHIROGANE");
    strcpy(opinfo_params.operator_code, "SR-GN");
    strcpy(opinfo_params.barcode_text, "SHIROGANE-SRGN123");
    strcpy(opinfo_params.staff_text, "Staff");
    strcpy(opinfo_params.aux_text, "Operator of Rhodes Island\nSUPPORTER/Rhodes Island\n我能吞下玻璃而不伤身体疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼疼");
    strcpy(opinfo_params.class_path, "/root/sniper.png");
    strcpy(opinfo_params.logo_path, "/root/ak_logo.png");
    overlay_opinfo_load_image(&opinfo_params);
    overlay_opinfo_show_arknights(&g_overlay, &opinfo_params);

    ui_warning(UI_WARNING_LOW_BATTERY);
    ui_warning(UI_WARNING_ASSET_ERROR);
    ui_warning(UI_WARNING_SD_MOUNT_ERROR);
    ui_warning(UI_WARNING_LOW_BATTERY);
    ui_warning(UI_WARNING_ASSET_ERROR);
    ui_warning(UI_WARNING_SD_MOUNT_ERROR);
    ui_warning(UI_WARNING_LOW_BATTERY);
    ui_warning(UI_WARNING_ASSET_ERROR);
    ui_warning(UI_WARNING_SD_MOUNT_ERROR);

    usleep(5 * 1000 * 1000);
    


    overlay_opinfo_stop(&g_overlay);

    // ============ 主循环 ===============
    // does nothing, stuck here until signal is received
    while(g_running){
        usleep(1 * 1000 * 1000);
    }

    overlay_opinfo_free_image(&opinfo_params);
    overlay_transition_free_image(&fade_params);

    /* cleanup */
    log_info("==========> Shutting down EPass DRM APP!");
    prts_timer_destroy(&g_prts_timer);
    lvgl_drm_warp_destroy(&g_lvgl_drm_warp);
    overlay_destroy(&g_overlay);
    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_destroy(&g_mediaplayer);
    drm_warpper_destroy(&g_drm_warpper);
    return g_exitcode;
}