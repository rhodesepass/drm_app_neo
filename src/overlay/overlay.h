#pragma once
#include "driver/drm_warpper.h"
#include "render/layer_animation.h"
#include <pthread.h>
#include "utils/timer.h"

typedef enum {
    OVERLAY_WORKER_MODE_TRANSITION,
    OVERLAY_WORKER_MODE_OPINFO,
} overlay_worker_mode_t;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int running;

    int in_progress;
    int pending;

    void (*func)(void *userdata,int skipped_frames);
    void* userdata;
    int skipped_frames;
    // opinfo
} overlay_worker_t;

typedef struct {
    drm_warpper_t* drm_warpper;
    layer_animation_t* layer_animation;
    buffer_object_t overlay_buf_1;
    buffer_object_t overlay_buf_2;
    drm_warpper_queue_item_t overlay_buf_1_item;
    drm_warpper_queue_item_t overlay_buf_2_item;

    overlay_worker_t worker;

    prts_timer_handle_t overlay_timer_handle;
} overlay_t;


int load_img_assets(char *image_path, uint32_t** addr,int* w,int* h);
uint64_t get_us(void);
int overlay_init(overlay_t* overlay,drm_warpper_t* drm_warpper,layer_animation_t* layer_animation);
int overlay_destroy(overlay_t* overlay);

void overlay_worker_schedule(overlay_t* overlay,void (*func)(void *userdata,int skipped_frames),void* userdata);
void overlay_poll_wait(overlay_t* overlay);