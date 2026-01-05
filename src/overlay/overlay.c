#include "overlay.h"
#include "config.h"
#include "drm_warpper.h"
#include "stdint.h"
#include "srgn_drm.h"
#include "stdbool.h"
#include "string.h"
#include "log.h"
#include "timer.h"
#include "layer_animation.h"

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

    log_info("==============> Overlay Initialized!");
    return 0;
}

int overlay_destroy(overlay_t* overlay){
    drm_warpper_free_buffer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_1);
    drm_warpper_free_buffer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_2);
    return 0;
}

// 程序启动动画
void overlay_schedule_startup_animation(overlay_t* overlay,void (*mount_layer_cb)(void *userdata,bool is_last)){
    drm_warpper_queue_item_t* item;

    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, OVERLAY_WIDTH, 0);
    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);

    // get a free buffer to draw on
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    for(int y=0; y<OVERLAY_HEIGHT; y++){
        for(int x=0; x<OVERLAY_WIDTH; x++){
            *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = 0xFFFFFFFF;
        }
    }

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

     

    // 用Overlay层，先从屏幕右侧进入
    layer_animation_ease_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        OVERLAY_WIDTH, 0, 
        0, 0, 
        500 * 1000, 
        0
    );

    // 一边进入一边渐变到白色
    layer_animation_fade_in(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        500 * 1000, 
        0
    );

    // 全部遮住以后挂载video层
    prts_timer_handle_t init_handler;
    prts_timer_create(&init_handler,500*1000,0,1,mount_layer_cb,NULL);

    // 渐变到透明
    layer_animation_fade_out(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        500 * 1000, 
        1000 * 1000
    );


}