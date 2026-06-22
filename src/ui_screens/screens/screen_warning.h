#pragma once
#include <lvgl/lvgl.h>
lv_obj_t *screen_warning_create(void);
// 模态服务：设置文案并切到告警屏。
void screen_warning_show(const char *icon, const char *title, const char *desc);
