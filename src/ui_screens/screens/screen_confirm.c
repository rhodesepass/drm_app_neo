#include "screen_confirm.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"
#include "icons.h"

static struct {
    lv_obj_t *head;
    lv_obj_t *title;
    char h[64];
    char t[128];
    bool t_large;   // 第二行字号 (show_impl 记录,create 懒建时也要按它应用)
    lv_obj_t *menu_btn;
    void (*on_proceed)(void);
    void (*on_cancel)(void);
} self;

static void dismiss_cancel(void)
{
    void (*cb)(void) = self.on_cancel;
    self.on_cancel = NULL;
    self.on_proceed = NULL;
    screen_show(SCREEN_SPINNER);
    if (cb) cb();
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    dismiss_cancel();
}
static void on_proceed(lv_event_t *e)
{
    (void)e;
    void (*cb)(void) = self.on_proceed;
    self.on_cancel = NULL;
    self.on_proceed = NULL;
    screen_show(SCREEN_SPINNER);
    if (cb) cb();
}

lv_obj_t *screen_confirm_create(void)
{
    lv_obj_t *root = ui_screen_root();
    add_style_fill(root, UI_SEM_WARNING);

    lv_obj_t *icon = lv_label_create(root);
    ui_place(icon, 14, 4, UI_OF_SLOT_CONFIRM_ICON); add_style_fa_label(icon);
    lv_label_set_text(icon, UI_ICON_TRIANGLE_EXCLAMATION);

    self.head = lv_label_create(root);
    ui_place(self.head, 83, 4, UI_OF_SLOT_CONFIRM_HEAD); add_style_label_large(self.head);
    lv_obj_set_style_text_color(self.head, ui_color(UI_C_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.head, self.h);

    self.title = lv_label_create(root);
    ui_place(self.title, 83, 37, UI_OF_SLOT_CONFIRM_TITLE); lv_obj_set_width(self.title, S(262));
    set_style_label_size(self.title, self.t_large);
    lv_obj_set_style_text_color(self.title, ui_color(UI_C_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.title, self.t);

    lv_obj_t *btns[2];
    // 基础布局 = 旧版坐标+文字 (取消/确定)。主题类表可覆盖按钮文字 (label_text)，
    // ui_place 叠加坐标/尺寸偏移 (见 UI_OF_SLOT_CONFIRM_*)。
    btns[0] = ui_text_button(root, 28, 70, 149, 51, UI_SEM_NEUTRAL, "取消", on_cancel);
    ui_place(btns[0], 28, 70, UI_OF_SLOT_CONFIRM_CANCEL);
    btns[1] = ui_text_button(root, 187, 69, 147, 52, UI_SEM_DANGER, "确定", on_proceed);
    ui_place(btns[1], 187, 69, UI_OF_SLOT_CONFIRM_OK);
    self.menu_btn = btns[1];

    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = btns[i];
        add_class(b, i == 0 ? UI_CLS_BTN_CANCEL : UI_CLS_BTN_CONFIRM);
    }

    return root;
}

void screen_confirm_show(const char *title, void (*proceed)(void))
{
    screen_confirm_show2("=PRTS二次确认=", title ? title : "确认操作?", proceed, NULL);
}

static void show_impl(const char *head, const char *desc,
                      void (*proceed)(void), void (*cancel)(void), bool desc_large)
{
    lv_strlcpy(self.h, head ? head : "=PRTS二次确认=", sizeof(self.h));
    lv_strlcpy(self.t, desc ? desc : "确认操作?", sizeof(self.t));
    self.t_large = desc_large;
    self.on_proceed = proceed;
    self.on_cancel = cancel;
    if (self.head) lv_label_set_text(self.head, self.h);
    if (self.title) {
        lv_label_set_text(self.title, self.t);
        set_style_label_size(self.title, desc_large);
    }
    screen_show(SCREEN_CONFIRM);
}

void screen_confirm_show2(const char *head, const char *desc,
                          void (*proceed)(void), void (*cancel)(void))
{
    show_impl(head, desc, proceed, cancel, true);
}

void screen_confirm_show_uix(const char *head, const char *desc,
                             void (*proceed)(void), void (*cancel)(void))
{
    show_impl(head, desc, proceed, cancel, false);
}

void screen_confirm_escape(void)
{
    dismiss_cancel();
}
