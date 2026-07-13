#pragma once
//
// ui_services —— 手写 UI 的跨线程服务 API (取代 EEZ 时代散在 actions_warning/confirm/
// scr_transition 里的对外入口)。
//
// 这些函数可被任意线程调用 (prts/apps/battery/ipc/main)。LVGL 非线程安全 ⇒ 内部一律
// 入 spsc 队列，由 LVGL 线程上的 lv_timer 取出后再碰 UI。设备侧实现见 platform/ui_services_device.c；
// sim 不需要 (无这些子系统)。
//
#include "ui_enums.h"   // curr_screen_t
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- 告警 ----
typedef enum {
    UI_WARNING_NONE = 0,
    UI_WARNING_LOW_BATTERY = 1,
    UI_WARNING_ASSET_ERROR = 2,
    UI_WARNING_SD_MOUNT_ERROR = 3,
    UI_WARNING_PRTS_CONFLICT = 4,
    UI_WARNING_NO_ASSETS = 5,
    UI_WARNING_NOT_IMPLEMENTED = 6,
    UI_WARNING_APP_NO_DIRECT_START = 7,
    UI_WARNING_APP_LOAD_ERROR = 8,
    UI_WARNING_APP_ALREADY_RUNNING = 9,
} warning_type_t;

void ui_warning(warning_type_t type);
void ui_warning_custom(char *title, char *desc, char *icon, uint32_t color);

// ---- 二次确认 ----
typedef enum {
    UI_CONFIRM_TYPE_FORMAT_SD_CARD,
    UI_CONFIRM_TYPE_SHUTDOWN,
} ui_confirm_type_t;

void ui_confirm(ui_confirm_type_t type);

// ---- 切屏 / 显图 (供 IPC) ----
void ui_schedule_screen_transition(curr_screen_t to_screen);
void ui_displayimg_force_dispimg(const char *path);
void ui_displayimg_rescan(void); // 重扫 /dispimg 目录 (IPC 素材刷新联动)

// ---- 查询 ----
curr_screen_t ui_get_current_screen(void);
bool          ui_is_hidden(void);   // 当前是否 spinner(隐藏)

// ---- 生命周期 (LVGL 线程内调；建队列与 timer) ----
void ui_services_init(void);
void ui_services_destroy(void);

#ifdef __cplusplus
}
#endif
