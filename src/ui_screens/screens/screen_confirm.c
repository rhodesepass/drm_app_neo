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
    lv_obj_t *cancel_button;
    lv_obj_t *proceed_button;
    bool shutdown;
    char h[64];
    char t[128];
    bool t_large;   // 第二行字号 (show_impl 记录,create 懒建时也要按它应用)
    void (*on_proceed)(void);
    void (*on_cancel)(void);
} self;
static bool s_touch;
static bool s_buttons = true;

static void update_layout(void)
{
    if (!self.title) return;
    lv_label_set_text(self.head, self.h);
    lv_label_set_text(self.title, self.t);
    set_style_label_size(self.title, self.t_large);
    if (self.shutdown) {
        lv_obj_add_flag(self.cancel_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(self.proceed_button, LV_OBJ_FLAG_HIDDEN);
        lv_group_remove_obj(self.cancel_button);
        lv_group_remove_obj(self.proceed_button);
    } else {
        lv_obj_remove_flag(self.cancel_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(self.proceed_button, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_shutdown_text(void)
{
    const char *hint = s_touch ? (s_buttons ? "下滑确认关机\n按3确认关机\n按电源键取消" : "下滑确认关机\n按电源键取消")
                               : (s_buttons ? "按3确认关机\n按电源键取消" : "按电源键取消");
    lv_strlcpy(self.t, hint, sizeof(self.t));
    update_layout();
}

static void focus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_editing(g, false);
    if (!self.shutdown) {
        lv_group_add_obj(g, self.cancel_button);
        lv_group_add_obj(g, self.proceed_button);
    }
}

static void dismiss_cancel(void)
{
    void (*cb)(void) = self.on_cancel;
    self.on_cancel = NULL;
    self.on_proceed = NULL;
    self.shutdown = false;
    screen_show(SCREEN_SPINNER);
    if (cb) cb();
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    if (self.shutdown) return;
    dismiss_cancel();
}
static void on_proceed(lv_event_t *e)
{
    (void)e;
    if (self.shutdown) return;
    void (*cb)(void) = self.on_proceed;
    self.on_cancel = NULL;
    self.on_proceed = NULL;
    screen_show(SCREEN_SPINNER);
    if (cb) cb();
}

lv_obj_t *screen_confirm_create(void)
{
    lv_obj_t *root = ui_screen_root_bare();
    lv_obj_add_event_cb(root, focus_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
    add_style_fill(root, UI_SEM_WARNING);

    lv_obj_t *icon = lv_label_create(root);
    lv_obj_set_pos(icon, S(14), S(4)); add_style_fa_label(icon);
    lv_label_set_text(icon, UI_ICON_TRIANGLE_EXCLAMATION);

    self.head = lv_label_create(root);
    lv_obj_set_pos(self.head, S(83), S(4)); add_style_label_large(self.head);
    lv_label_set_text(self.head, self.h);

    self.title = lv_label_create(root);
    lv_obj_set_pos(self.title, S(83), S(37)); lv_obj_set_width(self.title, S(262));
    set_style_label_size(self.title, self.t_large);
    lv_label_set_text(self.title, self.t);

    self.cancel_button = ui_text_button(root, 28, 70, 149, 51, UI_SEM_NEUTRAL, "取消", on_cancel);
    self.proceed_button = ui_text_button(root, 187, 69, 147, 52, UI_SEM_DANGER, "确定", on_proceed);
    add_style_focus(self.cancel_button);
    add_style_focus(self.proceed_button);
    update_layout();

    return root;
}

void screen_confirm_show(const char *title, void (*proceed)(void))
{
    screen_confirm_show2("=PRTS二次确认=", title ? title : "确认操作?", proceed, NULL);
}

static void show_impl(const char *head, const char *desc,
                      void (*proceed)(void), void (*cancel)(void), bool desc_large)
{
    if (screen_confirm_is_shutdown()) return;
    self.shutdown = false;
    lv_strlcpy(self.h, head ? head : "=PRTS二次确认=", sizeof(self.h));
    lv_strlcpy(self.t, desc ? desc : "确认操作?", sizeof(self.t));
    self.t_large = desc_large;
    self.on_proceed = proceed;
    self.on_cancel = cancel;
    update_layout();
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
    if (self.shutdown) return;
    dismiss_cancel();
}

bool screen_confirm_is_shutdown(void)
{
    return self.shutdown && screens_current() == SCREEN_CONFIRM;
}

void screen_confirm_show_shutdown(void (*proceed)(void), void (*cancel)(void))
{
    if (screen_confirm_is_shutdown()) return;
    self.shutdown = true;
    self.t_large = false;
    self.on_proceed = proceed;
    self.on_cancel = cancel;
    lv_strlcpy(self.h, "确定要关机吗？", sizeof(self.h));
    update_shutdown_text();
    screen_show(SCREEN_CONFIRM);
    if (screens_group()) lv_group_remove_all_objs(screens_group());
}

void screen_confirm_set_input_sources(bool touch, bool buttons)
{
    if (s_touch == touch && s_buttons == buttons) return;
    s_touch = touch;
    s_buttons = buttons;
    if (self.shutdown) update_shutdown_text();
}

void screen_confirm_shutdown_proceed(void)
{
    if (!screen_confirm_is_shutdown()) return;
    void (*cb)(void) = self.on_proceed;
    self.on_proceed = NULL;
    self.on_cancel = NULL;
    self.shutdown = false;
    screen_show(SCREEN_SPINNER);
    if (cb) cb();
}

void screen_confirm_shutdown_cancel(void)
{
    if (screen_confirm_is_shutdown()) dismiss_cancel();
}
