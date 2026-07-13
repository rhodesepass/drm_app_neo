#include <apps/apps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ui_screens/ui_services.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>
#ifndef _WIN32
#include <sys/wait.h>  // Windows 无子进程回收（后台 app 走 POSIX，见 apps.c）
#endif

#include "config.h"
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "utils/compat.h"
#include "render/mediaplayer.h"
#include "render/lvgl_drm_warp.h"
#include "overlay/overlay.h"
#include "utils/timer.h"
#include "render/layer_animation.h"
#include "utils/settings.h"
#include "utils/timer.h"
#include "utils/cacheassets.h"
#include "utils/respath.h"
#include "prts/prts.h"
#include "utils/misc.h"

/* global variables */
drm_warpper_t g_drm_warpper;
mediaplayer_t g_mediaplayer;
lvgl_drm_warp_t g_lvgl_drm_warp;
prts_timer_t g_prts_timer;
layer_animation_t g_layer_animation;
settings_t g_settings;
overlay_t g_overlay;
cacheassets_t g_cacheassets;
prts_t g_prts;
apps_t g_apps;

int g_running = 1;
int g_exitcode = 0;
bool g_use_sd = false;

void signal_handler(int sig)
{
    log_info("received signal %d, shutting down", sig);
    g_running = 0;
    g_exitcode = 0;
}

// video 层按解码尺寸记录挂载几何(惰性：plane 由显示线程随首帧启用，
// 不占黑 buffer；stop 后 plane disable，露出 DEBE 黑背景)。
// 返回 -1 表示不支持的尺寸
//
// vdec fb 宽为 VE 输出宽(32 对齐)，通常 > 屏幕真实宽度。统一走 src 裁窗 +
// dst=屏幕真实尺寸:
//   当代素材:crop 左 SCREEN_WIDTH×SCREEN_HEIGHT，1:1 不缩放(裁掉右侧 padding)。
//   旧素材(384x640,真实 360x640)@720 档:crop 左 360×640，DEFE 放大到 720×1280(等比 2x)。
//   高清素材(736x1280,真实 720x1280)@360 档:crop 左 720×1280，DEFE 缩小到 360×640(等比 1/2)。
//   padding 均被裁窗排除不参与缩放采样。
int video_layer_ensure_mount(int src_w, int src_h){
    if (src_w == VIDEO_WIDTH && src_h == VIDEO_HEIGHT) {
        drm_warpper_set_layer_geometry(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0,
                                       SCREEN_WIDTH, SCREEN_HEIGHT,
                                       SCREEN_WIDTH, SCREEN_HEIGHT);
#ifdef VIDEO_LEGACY_WIDTH
    } else if (src_w == VIDEO_LEGACY_WIDTH && src_h == VIDEO_LEGACY_HEIGHT) {
        drm_warpper_set_layer_geometry(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0,
                                       VIDEO_LEGACY_CROP_WIDTH, VIDEO_LEGACY_HEIGHT,
                                       SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
#ifdef VIDEO_HIRES_WIDTH
    } else if (src_w == VIDEO_HIRES_WIDTH && src_h == VIDEO_HIRES_HEIGHT) {
        drm_warpper_set_layer_geometry(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0,
                                       VIDEO_HIRES_CROP_WIDTH, VIDEO_HIRES_HEIGHT,
                                       SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
    } else {
        log_error("unsupported video size %dx%d", src_w, src_h);
        return -1;
    }
    return 0;
}

void mount_video_layer_callback(void *userdata,bool is_last){
    mediaplayer_remount_video_layer(&g_mediaplayer);
    drm_warpper_set_layer_coord(&g_drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0,0);
}


// ============ 组件依赖关系： ============
// LayerAnimation 依赖 PRTS定时器 与 drm warpper
// mediaplayer 依赖 drm warpper
// overlay 依赖 drm warpper 与 layer animation
// prts 依赖 overlay
// apps 依赖 prts
// lvgl_drm_warp 依赖 drm_warpper,prts,apps

// ============ 组件初始化顺序： ============
// 1. drm warpper
// 2. prts timer
// 3. settings
// 4. layer animation
// 5. mediaplayer
// 6. cacheassets
// 7. overlay
// 8. prts
// 9. apps
// 10. lvgl_drm_warp

int main(int argc, char *argv[]){
    if(argc == 2){
        if(strcmp(argv[1], "version") == 0){
            printf("APP_VERSION: %s\n", APP_VERSION_STRING);
            printf("COMPILE_TIME: %s\n", COMPILE_TIME);
            return 0;
        }
    }

    // 不再依赖启动参数：/sd 已挂载即启用 SD 资产扫描 (挂载由 init 脚本完成)
    g_use_sd = is_sd_mounted();

#ifdef APP_RELEASE
    log_warn("Release mode is enabled. Most logs are disabled.");
#endif // APP_RELEASE

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stderr,"APP_VERSION: %s\n", APP_VERSION_STRING);
    fprintf(stderr,"COMPILE_TIME: %s\n", COMPILE_TIME);

    fputs(APP_BARNER, stderr);
    log_info("==> Starting EPass DRM APP!");

    // ============ 资源目录解析 ===============
    // 解析可执行文件同级 res/ 目录, 后续所有内置资源路径都基于它。
    respath_init();

    // ============ DRM Warpper 初始化 ===============
    drm_warpper_init(&g_drm_warpper);

    // ============ PRTS 计时器 初始化 ===============
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

    // 老 FIXME 里那两块"跑 modeset 用"的黑 buffer 已优化掉：video 层惰性
    // 挂载(首帧随 mount 属性一起 commit)，stop 后 disable plane 露出黑背景

    /* initialize mediaplayer */
    if (mediaplayer_init(&g_mediaplayer, &g_drm_warpper) != 0) {
        log_error("failed to initialize mediaplayer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }

    // mediaplayer_set_video(&g_mediaplayer, "/assets/MS/loop.mp4");
    // mediaplayer_start(&g_mediaplayer);

    // ============ 缓冲素材 初始化 ===============
    // 原计划是用video层的buffer来缓存素材，但这样会导致第一次切换的时候闪烁，挺难看的...
    // 单独开了一个buffer
    uint8_t* cache_buf = malloc(CACHED_ASSETS_MAX_SIZE);
    if(!cache_buf){
        log_error("failed to allocate cache buffer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }
    cacheassets_init(&g_cacheassets, cache_buf, CACHED_ASSETS_MAX_SIZE);
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_AK_BAR, (char *)respath(CACHED_ASSETS_FILE_AK_BAR));
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_BTM_LEFT_BAR, (char *)respath(CACHED_ASSETS_FILE_BTM_LEFT_BAR));
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_LEFT_RECT, (char *)respath(CACHED_ASSETS_FILE_TOP_LEFT_RECT));
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_LEFT_RHODES, (char *)respath(CACHED_ASSETS_FILE_TOP_LEFT_RHODES));
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_RIGHT_BAR, (char *)respath(CACHED_ASSETS_FILE_TOP_RIGHT_BAR));
    cacheassets_put_asset(&g_cacheassets, CACHE_ASSETS_TOP_RIGHT_ARROW, (char *)respath(CACHED_ASSETS_FILE_TOP_RIGHT_ARROW));

    log_info("Cached assets: %d", g_cacheassets.curr_size);

    // ============ OVERLAY 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_OVERLAY, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_ARGB8888
    );

    overlay_init(&g_overlay, &g_drm_warpper, &g_layer_animation);

    // ============ PRTS 初始化===============
    prts_init(&g_prts, &g_overlay, g_use_sd);

    // ============ APPS 初始化 ===============
    apps_init(&g_apps, &g_prts, g_use_sd);

    // ============ LVGL 初始化 ===============
    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_UI, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_RGB565
    );

    lvgl_drm_warp_init(&g_lvgl_drm_warp, &g_drm_warpper,&g_layer_animation,&g_prts,&g_apps);
    // drm_warpper_set_layer_coord(&g_drm_warpper, DRM_WARPPER_LAYER_UI, 0, SCREEN_HEIGHT);
    // drm_warpper_set_layer_coord(&g_drm_warpper, DRM_WARPPER_LAYER_OVERLAY, OVERLAY_WIDTH, 0);

    // 如果SD卡插入，但没有启用SD模式，则应该是mount出错了，这边告警
    if(is_sdcard_inserted() && !g_use_sd){
        ui_warning(UI_WARNING_SD_MOUNT_ERROR);
    }


    // ============ 主循环 ===============
    while(g_running){
#ifndef _WIN32
        int status;
        pid_t pid;
        // 处理子进程（后台app）退出
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                log_info("child process %d exited with status %d", pid, WEXITSTATUS(status));
            }
        }
#endif
        usleep(1 * 1000 * 1000);
    }

    /* cleanup */
    log_info("==> Shutting down EPass DRM APP!");
    prts_timer_destroy(&g_prts_timer);
    apps_destroy(&g_apps);
    lvgl_drm_warp_destroy(&g_lvgl_drm_warp);
    overlay_destroy(&g_overlay);
    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_destroy(&g_mediaplayer);
    drm_warpper_destroy(&g_drm_warpper);
    settings_destroy(&g_settings);

    free(cache_buf);    
    return g_exitcode;
}