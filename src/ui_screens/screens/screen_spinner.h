#pragma once
#include <lvgl/lvgl.h>
lv_obj_t *screen_spinner_create(void);

// 叫停转圈的 lv_anim。停靠(隐藏)期它每 33ms invalidate 一次，屏幕外白转。
// 停了就回不来，调用方须 screens_rebuild(SCREEN_SPINNER) 让下次显示时重建。
void screen_spinner_stop_anim(void);
