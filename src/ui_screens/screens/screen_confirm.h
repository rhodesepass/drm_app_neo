#pragma once
#include <lvgl/lvgl.h>
lv_obj_t *screen_confirm_create(void);
// 模态服务：设置标题与确认回调并切到确认屏。
void screen_confirm_show(const char *title, void (*on_proceed)(void));
// 扩展版：自定义页头 + 描述，"取消"也有回调（UIX 外部确认要回传否）。cancel 可为 NULL。
void screen_confirm_show2(const char *head, const char *desc,
                          void (*proceed)(void), void (*cancel)(void));
// UIX/FIDO 确认：第二行用正文字号 (FONT_BODY)，取消/退回/超时回 spinner。
void screen_confirm_show_uix(const char *head, const char *desc,
                             void (*proceed)(void), void (*cancel)(void));
// ESC / 退回：等同点"取消"，收屏回 spinner。
void screen_confirm_escape(void);
