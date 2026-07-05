#pragma once
#include <lvgl/lvgl.h>
#include <stdint.h>

lv_obj_t *screen_usbselect_create(void);
// 模态服务：按 func_mask（bit0=MTP bit1=EPASS bit2=FIDO bit3=仅充电）显示选项并切屏。
// 用户点击后 uix_session_finish(session_id, UIX_CONFIRMED, choice) 并回主菜单。
void screen_usbselect_show(uint32_t session_id, uint32_t func_mask);
