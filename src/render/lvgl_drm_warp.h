#pragma once

#include "lvgl.h"
#include "drm_warpper.h"
#include <pthread.h>
#include <stdbool.h>

typedef struct {
    drm_warpper_t *drm_warpper;

    lv_disp_t* disp;

    bool has_vsync_done;

    buffer_object_t ui_buf;
    lv_disp_draw_buf_t disp_buf;
    lv_disp_drv_t disp_drv;


    lv_indev_drv_t evdev_drv;
    lv_indev_t* keypad_indev;

} lvgl_drm_warp_t;

void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper,uint8_t* draw_buf);
void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp);
void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp);