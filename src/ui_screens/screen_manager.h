#pragma once
//
// screen_manager —— 极小的屏导航/调度器，取代 EEZ 的 objects 索引表 + loadScreen。
//
// 每个屏是一个自包含模块 (screens/screen_xxx.c)，对外只暴露 create()/tick()。
// 这里维护 {create, tick, 缓存 obj} 一张表，外加键盘导航 group。
// 导航就一行：screen_show(SCREEN_OPLIST)。
//
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_MAINMENU = 0,
    SCREEN_OPLIST,
    SCREEN_SYSINFO,
    SCREEN_SPINNER,
    SCREEN_DISPLAYIMG,
    SCREEN_FILEMANAGER,
    SCREEN_SETTINGS,
    SCREEN_APPLIST,
    SCREEN_WARNING,
    SCREEN_CONFIRM,
    SCREEN_COUNT
} screen_id_t;

// 建 group、注册各屏、显示首屏 (须在 font_registry_init 之后)。
void screens_init(void);

// 切到目标屏：按其停靠 Y 调度 UI 平面滑动 (ui_plane seam) + 换 LVGL 内容。
void screen_show(screen_id_t id);

// 驱动当前屏的 tick (若该屏注册了 tick)。
void screens_tick(void);

// 当前屏。
screen_id_t screens_current(void);

// 键盘导航分组 (各屏在 SCREEN_LOAD_START 时把自己的可聚焦控件加进来)。
lv_group_t *screens_group(void);

// 编码器/按键导航状态机 (原 scr_transition.c 的 screen_key_event_cb)。
// 设备侧把硬件按键喂进来即可；sim 当前不驱动它 (维持鼠标点按 + group)。
void screens_handle_key(uint32_t key);

// ---- 平台服务钩子 (弱符号默认空实现，设备侧用强符号覆盖) ----
// 关机确认 (设备 -> ui_confirm(SHUTDOWN))
void ui_hook_shutdown_request(void);
// 扩列图按键 (设备 -> ui_displayimg_key_event)
void ui_hook_displayimg_key(uint32_t key);

#ifdef __cplusplus
}
#endif
