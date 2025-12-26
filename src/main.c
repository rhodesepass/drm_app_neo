#include "drm_warpper.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "log.h"
#include <signal.h>
#include "mediaplayer.h"
#include "lvgl_drm_warp.h"

static drm_warpper_t g_drm_warpper;
static mediaplayer_t g_mediaplayer;
static lvgl_drm_warp_t g_lvgl_drm_warp;

static int g_running = 1;
void signal_handler(int sig)
{
    log_info("received signal %d, shutting down", sig);
    g_running = 0;
}

int main(int argc, char *argv[]){
    if(argc == 2){
        if(strcmp(argv[1], "version") == 0){
            log_info("EPASS_GIT_VERSION: %s", EPASS_GIT_VERSION);
            return 0;
        }
        else if(strcmp(argv[1], "aux") == 0){
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if(drm_warpper_init(&g_drm_warpper) != 0){
        log_error("failed to initialize DRM warpper");
        return -1;
    }

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
    buffer_object_t video_buf;
    drm_warpper_allocate_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, &video_buf);
    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0, &video_buf);

    /* initialize mediaplayer */
    if (mediaplayer_init(&g_mediaplayer, &g_drm_warpper) != 0) {
        log_error("failed to initialize mediaplayer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }

    mediaplayer_set_video(&g_mediaplayer, "/assets/MS/loop.mp4");
    mediaplayer_start(&g_mediaplayer);


    // ============ LVGL 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_UI, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_ARGB8888
    );

    lvgl_drm_warp_init(&g_lvgl_drm_warp, &g_drm_warpper);
    log_info("after create ui");

    log_info("drm_warpper addr:%p", &g_drm_warpper);

    // ============ 主循环 ===============
    while(g_running){
        lvgl_drm_warp_tick(&g_lvgl_drm_warp);
    }

    /* cleanup */
    log_info("shutting down");
    lvgl_drm_warp_destroy(&g_lvgl_drm_warp);
    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_destroy(&g_mediaplayer);
    drm_warpper_destroy(&g_drm_warpper);
    return 0;
}