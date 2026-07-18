#pragma once
#include <lvgl/lvgl.h>
#include <stdbool.h>
lv_obj_t *screen_oplist_create(void);

// 干员列表是否处于排序模式(供 screen_manager 拦截方向键/ESC)。
bool screen_oplist_is_reordering(void);
// 退出排序模式；save=true 则把新顺序落盘。
void screen_oplist_exit_reorder(bool save);
// 排序模式下方向键移动被拎干员(LV_KEY_LEFT 上移 / LV_KEY_RIGHT 下移)。
void screen_oplist_reorder_move(uint32_t key);
