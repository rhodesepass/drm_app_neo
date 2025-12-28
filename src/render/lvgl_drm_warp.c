#include "lvgl_drm_warp.h"
#include "config.h"
#include "log.h"
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include "gui_app.h"

static uint32_t lvgl_drm_warp_tick_get_cb(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time_ms = t.tv_sec * 1000 + (t.tv_nsec / 1000000);
    return time_ms;
}

static void lvgl_drm_warp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{

    if(!lv_disp_flush_is_last(disp)){
        lv_display_flush_ready(disp);
        return;
    }
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);

    

    // log_info("enqueue display item");

    if(lvgl_drm_warp->curr_draw_buf_idx == 0){
        drm_warpper_enqueue_display_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1_item);
    }else{
        drm_warpper_enqueue_display_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2_item);
    }
    lvgl_drm_warp->curr_draw_buf_idx = !lvgl_drm_warp->curr_draw_buf_idx;

    // lvgl_drm_warp->has_vsync_done = false;

    // wait for vsync done
    drm_warpper_queue_item_t* item;
    // log_info("waiting for vsync");
    drm_warpper_dequeue_free_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &item);
    // log_info("dequeued free item");
    // log_debug("flush_cb called, has_vsync_done: %d -> false", lvgl_drm_warp->has_vsync_done);
    lv_display_flush_ready(disp);
}

// 这个回调函数呢，他什么时候都可能来调用一下，**就算现在没有正在刷新的内容 他也会来调用一下....**
// 所以达到vsync以后，之后就不能dequeue(也就是等待了)
// static void lvgl_drm_warp_flush_wait_cb(lv_display_t * disp){
//     drm_warpper_queue_item_t* item;
//     lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);
//     // log_debug("flush_wait_cb called, has_vsync_done: %d", lvgl_drm_warp->has_vsync_done);

//     if(lvgl_drm_warp->has_vsync_done){
//         return;
//     }

//     // dequeue only, act as "waiting for vsync"
//     // log_debug("waiting for vsync");
//     drm_warpper_dequeue_free_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &item);
//     // log_debug("dequeued free item");

//     lvgl_drm_warp->has_vsync_done = true;
// }


void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper){

    lvgl_drm_warp->drm_warpper = drm_warpper;

    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1);
    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2);

    // modeset
    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0, &lvgl_drm_warp->ui_buf_1);

    lvgl_drm_warp->ui_buf_1_item.mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
    lvgl_drm_warp->ui_buf_1_item.mount.arg0 = (uint32_t)lvgl_drm_warp->ui_buf_1.vaddr;
    lvgl_drm_warp->ui_buf_1_item.mount.arg1 = 0;
    lvgl_drm_warp->ui_buf_1_item.mount.arg2 = 0;
    lvgl_drm_warp->ui_buf_1_item.userdata = (void*)&lvgl_drm_warp->ui_buf_1;

    lvgl_drm_warp->ui_buf_2_item.mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
    lvgl_drm_warp->ui_buf_2_item.mount.arg0 = (uint32_t)lvgl_drm_warp->ui_buf_2.vaddr;
    lvgl_drm_warp->ui_buf_2_item.mount.arg1 = 0;
    lvgl_drm_warp->ui_buf_2_item.mount.arg2 = 0;
    lvgl_drm_warp->ui_buf_2_item.userdata = (void*)&lvgl_drm_warp->ui_buf_2;

    lvgl_drm_warp->has_vsync_done = true;

    // 先把buffer提交进去，形成队列的初始状态（有一个buffer等待被free回来）
    // drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1_item);
    drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2_item);
    

    lv_init();
    lv_tick_set_cb(lvgl_drm_warp_tick_get_cb);
    lvgl_drm_warp->curr_draw_buf_idx = 0;

    lv_display_t * disp;
    disp = lv_display_create(UI_WIDTH, UI_HEIGHT);
    lv_display_set_buffers(disp, 
        lvgl_drm_warp->ui_buf_1.vaddr,
        lvgl_drm_warp->ui_buf_2.vaddr, 
        UI_WIDTH * UI_HEIGHT * 4,
        LV_DISPLAY_RENDER_MODE_DIRECT);
    
    lvgl_drm_warp->disp = disp;
    lv_display_set_driver_data(disp, lvgl_drm_warp);
    lv_display_set_flush_cb(disp, lvgl_drm_warp_flush_cb);
    // lv_display_set_flush_wait_cb(disp, lvgl_drm_warp_flush_wait_cb);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    lvgl_drm_warp->keypad_indev = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, "/dev/input/event0");

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(lvgl_drm_warp->keypad_indev, g);

    gui_app_create_ui(lvgl_drm_warp);

}

void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp){
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1);
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2);
}

void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp){
    uint32_t idle_time = lv_timer_handler();
    usleep(idle_time * 1000);
}