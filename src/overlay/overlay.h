#pragma once
#include "drm_warpper.h"
#include "layer_animation.h"
#include "timer.h"

typedef struct {
    drm_warpper_t* drm_warpper;
    layer_animation_t* layer_animation;
    buffer_object_t overlay_buf_1;
    buffer_object_t overlay_buf_2;
    drm_warpper_queue_item_t overlay_buf_1_item;
    drm_warpper_queue_item_t overlay_buf_2_item;
} overlay_t;

int overlay_init(overlay_t* overlay,drm_warpper_t* drm_warpper,layer_animation_t* layer_animation);
int overlay_destroy(overlay_t* overlay);
void overlay_schedule_startup_animation(overlay_t* overlay,void (*mount_layer_cb)(void *userdata,bool is_last));