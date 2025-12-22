#include "drm_warpper.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "log.h"
#include <signal.h>
#include "mediaplayer.h"
#include "virt_to_phys.h"


drm_warpper_t g_drm_warpper;
mediaplayer_t g_mediaplayer;

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

    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_VIDEO, 
        VIDEO_WIDTH, 
        VIDEO_HEIGHT, 
    DRM_WARPPER_LAYER_MODE_MB32_NV12);
    
    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0);

    /* initialize mediaplayer */
    if (mediaplayer_init(&g_mediaplayer) != 0) {
        log_error("failed to initialize mediaplayer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }

    mediaplayer_set_output_buffer(&g_mediaplayer, g_drm_warpper.plane[DRM_WARPPER_LAYER_VIDEO].buf[0].vaddr);
    mediaplayer_set_video(&g_mediaplayer, "/assets/MS/loop.mp4");
    mediaplayer_start(&g_mediaplayer);

    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_UI, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_ARGB8888
    );
    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0);

    // getchar();
    // drm_warpper_switch_buffer_ioctl(&g_drm_warpper, DRM_WARPPER_LAYER_UI);
    // getchar();

    uint8_t *vaddr;
    while(g_running){
        log_info("acquiring draw buffer");
        drm_warpper_arquire_draw_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &vaddr);
        for(int i = 0; i < 100; i++){
            for(int j = 0; j < 100; j++){
                ((uint32_t*)vaddr)[i * 100 + j] = 0xff000000;
            }
        }
        log_info("returning draw buffer");
        drm_warpper_return_draw_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, vaddr);
        log_info("acquiring 2 draw buffer");
        drm_warpper_arquire_draw_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &vaddr);
        for(int i = 0; i < 100; i++){
            for(int j = 0; j < 100; j++){
                ((uint32_t*)vaddr)[i * 100 + j] = 0xff0000ff;
            }
        }
        log_info("returning 2 draw buffer");
        drm_warpper_return_draw_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, vaddr);
        usleep(1000);
    }

    /* cleanup */
    log_info("shutting down");

    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_destroy(&g_mediaplayer);
    drm_warpper_destroy(&g_drm_warpper);

    return 0;
}