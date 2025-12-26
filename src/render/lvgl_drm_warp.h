#pragma once

#include "lvgl.h"
#include "drm_warpper.h"
#include <pthread.h>

typedef struct {
    drm_warpper_t *drm_warpper;

    lv_display_t * disp;

    int curr_draw_buf_idx;

    buffer_object_t ui_buf_1;
    buffer_object_t ui_buf_2;
    drm_warpper_queue_item_t ui_buf_1_item;
    drm_warpper_queue_item_t ui_buf_2_item;

    bool has_vsync_done;
    lv_indev_t *keypad_indev;

} lvgl_drm_warp_t;

void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper);
void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp);
void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp);