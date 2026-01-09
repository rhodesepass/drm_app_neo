#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "overlay/overlay.h"
#include "config.h"
#include "driver/drm_warpper.h"
#include "driver/srgn_drm.h"
#include "utils/log.h"
#include "utils/timer.h"
#include "render/layer_animation.h"
#include "utils/stb_image.h"


int load_img_assets(char *image_path, uint32_t** addr,int* w,int* h){
    int c;

    if(strlen(image_path) == 0){
        *w = 0;
        *h = 0;
        *addr = NULL;
        return 0;
    }

    uint8_t* pixdata = stbi_load(image_path, w, h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", image_path);
        return -1;
    }
    *addr = malloc((*w) * (*h) * 4);
    if(!*addr){
        log_error("failed to malloc memory: %s", image_path);
        stbi_image_free(pixdata);
        pixdata = NULL;
        return -1;
    }
    for(int y = 0; y < (*h); y++){
        for(int x = 0; x < (*w); x++){
            uint32_t bgra_pixel = *((uint32_t *)(pixdata) + x + y * (*w));
            uint32_t rgb_pixel = (bgra_pixel & 0x000000FF) << 16 | (bgra_pixel & 0x0000FF00) | (bgra_pixel & 0x00FF0000) >> 16 | (bgra_pixel & 0xFF000000);
            *((uint32_t *)(*addr) + x + y * (*w)) = rgb_pixel;
        }
    }
    stbi_image_free(pixdata);
    pixdata = NULL;
    return 0;
}

uint64_t get_us(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time_us = t.tv_sec * 1000000 + (t.tv_nsec / 1000);
    return time_us;
}


// 我们向普瑞塞斯(timer)承诺了一切操作都会尽快完成，也就是说，定时器回调正在运行的时候，不应该再次触发定时器回调。
// 因此，在overlay层进行的一切耗时操作，都需要通过worker来完成
void* overlay_worker_thread(void* arg){
    overlay_t* overlay = (overlay_t*)arg;
    overlay_worker_t* worker = &overlay->worker;
    log_info("==> Overlay Worker Thread Started!");
    while(worker->running){
        pthread_mutex_lock(&worker->mutex);

        while(!worker->pending){
            pthread_cond_wait(&worker->cond, &worker->mutex);
            if(!worker->running){
                goto worker_end;
            }
        }

        worker->pending = 0;
        worker->in_progress = 1;
        // 这里不是原子性的，如果在耗时操作的时候还是发生了跳帧
        // 我们只能扣掉在这次func里处理过的跳帧，剩下的交给下一次处理。
        int processed_skipped_frame = worker->skipped_frames;

        pthread_mutex_unlock(&worker->mutex);

        // 不要持锁干耗时操作，否则堵塞prts_timer
        worker->func(worker->userdata,processed_skipped_frame);
        worker->skipped_frames -= processed_skipped_frame;

        pthread_mutex_lock(&worker->mutex);
        worker->in_progress = 0;
        pthread_mutex_unlock(&worker->mutex);
    }

worker_end:
    log_info("==> Overlay Worker Thread Ended!");
    return NULL;
}

void overlay_worker_schedule(overlay_t* overlay,void (*func)(void *userdata,int skipped_frames),void* userdata){
    overlay_worker_t* worker = &overlay->worker;
    pthread_mutex_lock(&worker->mutex);
    if(!worker->in_progress){
        worker->pending = 1;
        worker->func = func;
        worker->userdata = userdata;
        pthread_cond_signal(&worker->cond);
    }
    else{
        log_warn("overlay worker can't keep up... dropping task");
        worker->skipped_frames++;
    }
    pthread_mutex_unlock(&worker->mutex);
}

int overlay_init(overlay_t* overlay,drm_warpper_t* drm_warpper,layer_animation_t* layer_animation){

    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_1);
    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_2);

    memset(overlay->overlay_buf_1.vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    memset(overlay->overlay_buf_2.vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);

    overlay->overlay_buf_1_item.mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
    overlay->overlay_buf_1_item.mount.arg0 = (uint32_t)overlay->overlay_buf_1.vaddr;
    overlay->overlay_buf_1_item.mount.arg1 = 0;
    overlay->overlay_buf_1_item.mount.arg2 = 0;
    overlay->overlay_buf_1_item.userdata = (void*)&overlay->overlay_buf_1;
    overlay->overlay_buf_1_item.on_heap = false;

    overlay->overlay_buf_2_item.mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
    overlay->overlay_buf_2_item.mount.arg0 = (uint32_t)overlay->overlay_buf_2.vaddr;
    overlay->overlay_buf_2_item.mount.arg1 = 0;
    overlay->overlay_buf_2_item.mount.arg2 = 0;
    overlay->overlay_buf_2_item.userdata = (void*)&overlay->overlay_buf_2;
    overlay->overlay_buf_2_item.on_heap = false;

    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, OVERLAY_WIDTH, 0, &overlay->overlay_buf_1);

    // 先把两个buffer都提交一次，形成队列的初始状态（一个显示中，一个等待取回）
    drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_1_item);
    drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_2_item);

    overlay->drm_warpper = drm_warpper;
    overlay->layer_animation = layer_animation;

    overlay->worker.running = 1;
    pthread_create(&overlay->worker.thread, NULL, overlay_worker_thread, overlay);

    log_info("==> Overlay Initalized!");
    return 0;
}

int overlay_destroy(overlay_t* overlay){
    overlay->worker.running = 0;
    pthread_cond_signal(&overlay->worker.cond);
    pthread_join(overlay->worker.thread, NULL);
    drm_warpper_free_buffer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_1);
    drm_warpper_free_buffer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_2);
    return 0;
}


// 这个终止函数的调用者一般是 PRTS自己的timer回调
// PRTS那边定时器周期比较长，而且这边request stop，最差情况下应该是多渲染一帧之后再退出
// 现在我们还是用轮询的方式来做的。
// 如果卡住prts太久，导致那边定时器被双重触发，就需要想个办法处理一下。
void overlay_abort(overlay_t* overlay){
    overlay->request_abort = 1;
    while(overlay->overlay_timer_handle){
        usleep(50 * 1000);
    }
    return;
}