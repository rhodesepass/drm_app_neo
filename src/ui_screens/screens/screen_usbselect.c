//
// USB 功能选择屏 —— usb_aio_handler 检测到主机枚举（greeter ENABLE）后经 IPC 弹出。
// 用户选中一项即回填 uix_session 并回 spinner(隐藏)；不选则由 UIX 超时收屏。
//
#include "screen_usbselect.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"
#include "icons.h"
#include "ui/uix_session.h"

static struct {
    lv_obj_t *btn[4]; // 与 uix_usb_choice_t 下标一致
    uint32_t session_id;
} self;

static void pick(uint32_t choice)
{
    uint32_t id = self.session_id;
    self.session_id = 0;
    screen_show(SCREEN_SPINNER);
    uix_session_finish(id, UIX_CONFIRMED, choice);
}

static void on_mtp(lv_event_t *e)    { (void)e; pick(UIX_USB_CHOICE_MTP); }
static void on_epass(lv_event_t *e)  { (void)e; pick(UIX_USB_CHOICE_EPASS); }
static void on_fido(lv_event_t *e)   { (void)e; pick(UIX_USB_CHOICE_FIDO); }
static void on_charge(lv_event_t *e) { (void)e; pick(UIX_USB_CHOICE_CHARGE_ONLY); }

lv_obj_t *screen_usbselect_create(void)
{
    lv_obj_t *root = ui_screen_root();
    add_style_fill(root, UI_SEM_PRIMARY);

    lv_obj_t *icon = lv_label_create(root);
    lv_obj_set_pos(icon, S(14), S(4)); add_style_fa_label(icon);
    lv_label_set_text(icon, UI_ICON_MOBILE_SCREEN);

    lv_obj_t *head = lv_label_create(root);
    lv_obj_set_pos(head, S(83), S(4)); add_style_label_large(head);
    lv_label_set_text(head, "检测到USB连接");

    lv_obj_t *hint = lv_label_create(root);
    lv_obj_set_pos(hint, S(83), S(37)); lv_obj_set_width(hint, S(262));
    add_style_label_large(hint);
    lv_label_set_text(hint, "请选择本次连接的用途");

    self.btn[UIX_USB_CHOICE_EPASS]       = ui_text_button(root, 28, 80, 149, 51, UI_SEM_NEUTRAL, "管理APP", on_epass);
    self.btn[UIX_USB_CHOICE_FIDO]        = ui_text_button(root, 187, 80, 147, 51, UI_SEM_NEUTRAL, "FIDO密钥", on_fido);
    self.btn[UIX_USB_CHOICE_MTP]         = ui_text_button(root, 28, 137, 149, 51, UI_SEM_NEUTRAL, "文件/MTP", on_mtp);
    self.btn[UIX_USB_CHOICE_CHARGE_ONLY] = ui_text_button(root, 187, 137, 147, 51, UI_SEM_NEUTRAL, "仅充电", on_charge);

    return root;
}

void screen_usbselect_show(uint32_t session_id, uint32_t func_mask)
{
    self.session_id = session_id;
    if (func_mask == 0) func_mask = 0xF;
    for (int i = 0; i < 4; i++) {
        if (!self.btn[i]) continue;
        if (func_mask & (1u << i))
            lv_obj_remove_flag(self.btn[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(self.btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    screen_show(SCREEN_USBSELECT);
}
