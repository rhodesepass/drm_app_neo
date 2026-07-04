#pragma once
#include <lvgl/lvgl.h>
lv_obj_t *screen_confirm_create(void);
// 模态服务：设置标题与确认回调并切到确认屏。
void screen_confirm_show(const char *title, void (*on_proceed)(void));
