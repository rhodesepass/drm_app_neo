#include "lvgl_drm_warp.h"
#include "config.h"
#include "log.h"
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include "gui_app.h"
#include "evdev.h"
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include "drm_warpper.h"

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t lvgl_drm_warp_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

static void lvgl_drm_warp_flush_cb(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p){
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)drv->user_data;

    if( area->x2 < 0 ||
        area->y2 < 0 ||
        area->x1 > UI_WIDTH - 1 ||
        area->y1 > UI_HEIGHT - 1) {
        lv_disp_flush_ready(drv);
        return;
    }

    if(!lvgl_drm_warp->has_vsync_done){
        drm_warpper_wait_for_vsync(lvgl_drm_warp->drm_warpper);
        lvgl_drm_warp->has_vsync_done = true;
    }

    /*Truncate the area to the screen*/
    int32_t act_x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t act_y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t act_x2 = area->x2 > (int32_t)UI_WIDTH - 1 ? (int32_t)UI_WIDTH - 1 : area->x2;
    int32_t act_y2 = area->y2 > (int32_t)UI_HEIGHT - 1 ? (int32_t)UI_HEIGHT - 1 : area->y2;

    lv_coord_t w = (act_x2 - act_x1 + 1);
    long int location = 0;


    /*32 or 24 bit per pixel*/
    uint32_t * fbp32 = (uint32_t *)lvgl_drm_warp->ui_buf.vaddr;
    int32_t y;
    for(y = act_y1; y <= act_y2; y++) {
        location = (act_x1) + (y) * UI_WIDTH;
        memcpy(&fbp32[location], (uint32_t *)color_p, (act_x2 - act_x1 + 1) * 4);
        color_p += w;
    }


    if(lv_disp_flush_is_last(drv)){
        lvgl_drm_warp->has_vsync_done = false;
    }

    lv_disp_flush_ready(drv);
    return;
}


void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper,uint8_t* draw_buf){

    lvgl_drm_warp->drm_warpper = drm_warpper;

    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf);
    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0, &lvgl_drm_warp->ui_buf);

    
    lv_init();
    
    // 这里，video层的buffer是NV12,所以大小是像素数量的1.5倍，函数要的结果是像素为单位
    lv_disp_draw_buf_init(&lvgl_drm_warp->disp_buf, draw_buf, NULL, VIDEO_HEIGHT * VIDEO_WIDTH * 3 / 2 / 4);

    lv_disp_drv_init(&lvgl_drm_warp->disp_drv);
    lvgl_drm_warp->disp_drv.draw_buf   = &lvgl_drm_warp->disp_buf;
    lvgl_drm_warp->disp_drv.flush_cb   = lvgl_drm_warp_flush_cb;
    lvgl_drm_warp->disp_drv.hor_res    = UI_WIDTH;
    lvgl_drm_warp->disp_drv.ver_res    = UI_HEIGHT;
    lvgl_drm_warp->disp_drv.user_data  = lvgl_drm_warp;
    // lvgl_drm_warp->disp_drv.antialiasing = 1;
    lvgl_drm_warp->disp_drv.screen_transp = 1;

    lvgl_drm_warp->disp = lv_disp_drv_register(&lvgl_drm_warp->disp_drv);
    lvgl_drm_warp->has_vsync_done = false;

    evdev_init();
    lv_indev_drv_init(&lvgl_drm_warp->evdev_drv);
    lvgl_drm_warp->evdev_drv.type = LV_INDEV_TYPE_KEYPAD;
    lvgl_drm_warp->evdev_drv.read_cb = evdev_read;
    lvgl_drm_warp->keypad_indev = lv_indev_drv_register(&lvgl_drm_warp->evdev_drv);

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(lvgl_drm_warp->keypad_indev, g);

    gui_app_create_ui(lvgl_drm_warp);

}

void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp){
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf);
}

void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp){
    lv_timer_handler();
    usleep(5000);
}