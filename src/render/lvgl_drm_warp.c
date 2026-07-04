#include <apps/apps_types.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <ui/ipc_helper.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <lvgl/lvgl.h>
#include "render/lvgl_drm_warp.h"
#include "config.h"
#include "render/layer_animation.h"
#include "utils/log.h"
#include "driver/key_enc_evdev.h"
#include "ui/filemanager.h"
#include "ui/font_registry.h"
#include "ui/ui_theme.h"
#include "prts/prts.h"
#include "ui/battery.h"

#include "ui_screens/screen_manager.h"
#include "ui_screens/ui_backend.h"
#include "ui_screens/ui_services.h"
#include "ui_screens/ui_plane.h"

static uint32_t lvgl_drm_warp_tick_get_cb(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time_ms = t.tv_sec * 1000 + (t.tv_nsec / 1000000);
    return time_ms;
}

static void lvgl_drm_warp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);
    buffer_object_t *fb = &lvgl_drm_warp->ui_buf;

    // partial 模式：area 是脏矩形，px_map 里是紧凑排布(stride = 区域宽)的 RGB565。
    // 逐行 memcpy 进扫描 FB 对应位置(FB 行跨度用 pitch，可能含对齐 padding)。
    const int32_t x1 = area->x1;
    const int32_t y1 = area->y1;
    const int32_t h  = lv_area_get_height(area);
    const uint32_t line_bytes = (uint32_t)lv_area_get_width(area) * 2;

    uint8_t *dst = fb->vaddr + (uint32_t)y1 * fb->pitch + (uint32_t)x1 * 2;
    const uint8_t *src = px_map;
    for (int32_t y = 0; y < h; y++) {
        memcpy(dst, src, line_bytes);
        dst += fb->pitch;
        src += line_bytes;
    }

    lv_display_flush_ready(disp);
}

static void* lvgl_drm_warp_thread_entry(void *arg){
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)arg;
    log_info("==> LVGL Thread Started!");
    while(atomic_load(&lvgl_drm_warp->running)){
        uint32_t idle_time = lv_timer_handler();
        screens_tick();
        usleep(idle_time * 1000);
    }
    log_info("==> LVGL Thread Ended!");
    return NULL;
}


// key_enc_evdev 的 input_cb：转发到手写屏的导航状态机。
static void screen_key_event_cb(uint32_t key){
    screens_handle_key(key);
}
void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper,layer_animation_t *layer_animation,prts_t *prts,apps_t *apps){

    lvgl_drm_warp->drm_warpper = drm_warpper;
    lvgl_drm_warp->layer_animation = layer_animation;

    // 单 buffer 直绘(同 overlay)：只分配一块扫描 FB，挂载一次后不走 flip 队列。
    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf);
    memset(lvgl_drm_warp->ui_buf.vaddr, 0, lvgl_drm_warp->ui_buf.size);
    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_UI, 0, SCREEN_HEIGHT, &lvgl_drm_warp->ui_buf);

    lv_init();
    lv_tick_set_cb(lvgl_drm_warp_tick_get_cb);

    // partial 绘制暂存：约 1/10 屏，够容纳单次脏区渲染，超出部分 LVGL 自动分块多次 flush。
    const size_t partial_sz = (size_t)UI_WIDTH * (UI_HEIGHT / 10) * 2;
    lvgl_drm_warp->partial_buf = malloc(partial_sz);

    lv_display_t * disp;
    disp = lv_display_create(UI_WIDTH, UI_HEIGHT);
    lv_display_set_buffers(disp,
        lvgl_drm_warp->partial_buf,
        NULL,
        partial_sz,
        LV_DISPLAY_RENDER_MODE_PARTIAL);

    lvgl_drm_warp->disp = disp;
    lv_display_set_driver_data(disp, lvgl_drm_warp);
    lv_display_set_flush_cb(disp, lvgl_drm_warp_flush_cb);
    // lv_display_set_flush_wait_cb(disp, lvgl_drm_warp_flush_wait_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    lvgl_drm_warp->key_enc_evdev.input_cb = screen_key_event_cb;
    snprintf(lvgl_drm_warp->key_enc_evdev.dev_path, sizeof(lvgl_drm_warp->key_enc_evdev.dev_path), "/dev/input/event%d", 0);
    key_enc_evdev_init(&lvgl_drm_warp->key_enc_evdev);
    lvgl_drm_warp->keypad_indev = lvgl_drm_warp->key_enc_evdev.indev;

    // 手写 UI 起步：字体 -> 后端数据/动作 -> 平面滑动绑定 -> 建屏 -> 绑定导航 group。
    font_registry_init();
    ui_backend_init(prts, apps);
    ui_theme_apply(ui_backend_theme_get());   // 按存档应用深/浅主题(顺带把中文字体设成主题默认)
    ui_plane_device_bind(layer_animation);
    filemanager_init(apps);
    screens_init();
    lv_indev_set_group(lvgl_drm_warp->keypad_indev, screens_group());

    // 跨线程服务桥 (告警/确认队列 + timer) 与电量/IPC 组件
    ui_services_init();
    ui_battery_init();
    ui_ipc_helper_init();

    atomic_store(&lvgl_drm_warp->running, 1);
    if (pthread_create(&lvgl_drm_warp->lvgl_thread, NULL, lvgl_drm_warp_thread_entry, lvgl_drm_warp) != 0) {
        log_error("Failed to create LVGL thread");
        atomic_store(&lvgl_drm_warp->running, 0);
        return;
    }
    log_info("==> LVGL warpper Initalized!");
}

void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp){
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf);
    free(lvgl_drm_warp->partial_buf);

    atomic_store(&lvgl_drm_warp->running, 0);
    pthread_join(lvgl_drm_warp->lvgl_thread, NULL);
    key_enc_evdev_destroy(&lvgl_drm_warp->key_enc_evdev);
    ui_services_destroy();
    ui_battery_destroy();
    ui_ipc_helper_destroy();
}
