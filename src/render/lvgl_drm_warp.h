#pragma once

#include "lvgl.h"
#include "driver/drm_warpper.h"
#include <prts/prts.h>
#include <pthread.h>
#include <stdatomic.h>
#include "render/layer_animation.h"
#include "driver/key_enc_evdev.h"
#include "apps/apps_types.h"
typedef struct {
    drm_warpper_t *drm_warpper;
    layer_animation_t *layer_animation;

    lv_display_t * disp;

    // 单 buffer 直绘：挂载一次后 LVGL partial 渲染，flush 时按脏区拷进这块扫描帧缓冲。
    // 省掉了第二块整屏 FB(720x1280x2≈1.84M)，代价是绘制期可能和 DEBE 扫描撞上产生轻微撕裂。
    buffer_object_t ui_buf;
    // LVGL partial 模式的绘制暂存(堆内存，仅 CPU 用)，约 1/10 屏。
    uint8_t *partial_buf;

    lv_indev_t *keypad_indev;
    key_enc_evdev_t key_enc_evdev;

    pthread_t lvgl_thread;
    atomic_int running;

} lvgl_drm_warp_t;

void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper,layer_animation_t *layer_animation,prts_t* prts,apps_t *apps);
void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp);
void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp);