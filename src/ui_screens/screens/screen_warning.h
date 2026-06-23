#pragma once
#include <lvgl/lvgl.h>
#include <stdint.h>
lv_obj_t *screen_warning_create(void);
// 模态服务：设置文案/底色并切到告警屏。color 为 0 时沿用默认错误红。
void screen_warning_show(const char *icon, const char *title, const char *desc, uint32_t color);
