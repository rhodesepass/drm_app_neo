#pragma once
//
// 主菜单屏 (自包含：view + 自身事件回调 + tick 都在 .c 里)。
//
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_mainmenu_create(void);
void      screen_mainmenu_tick(void);

#ifdef __cplusplus
}
#endif
