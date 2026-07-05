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
    void (*on_proceed)(void);
    void (*on_cancel)(void);
} self;

static void on_cancel(lv_event_t *e)
{
    (void)e;
    void (*cb)(void) = self.on_cancel;
    self.on_cancel = NULL;
    screen_show(SCREEN_MAINMENU);
    if (cb) cb();
}
static void on_proceed(lv_event_t *e)
{
    (void)e;
    void (*cb)(void) = self.on_proceed;
    self.on_cancel = NULL;
    screen_show(SCREEN_MAINMENU);
    if (cb) cb();
}

lv_obj_t *screen_confirm_create(void)
{
    lv_obj_t *root = ui_screen_root();
    add_style_fill(root, UI_SEM_WARNING);

    lv_obj_t *icon = lv_label_create(root);
    lv_obj_set_pos(icon, S(14), S(4)); add_style_fa_label(icon);
    lv_label_set_text(icon, UI_ICON_TRIANGLE_EXCLAMATION);

    self.head = lv_label_create(root);
    lv_obj_set_pos(self.head, S(83), S(4)); add_style_label_large(self.head);
    lv_label_set_text(self.head, self.h);

    self.title = lv_label_create(root);
    lv_obj_set_pos(self.title, S(83), S(37)); lv_obj_set_width(self.title, S(262));
    add_style_label_large(self.title);
    lv_label_set_text(self.title, self.t);

    ui_text_button(root, 28, 70, 149, 51, UI_SEM_NEUTRAL, "取消", on_cancel);
    ui_text_button(root, 187, 69, 147, 52, UI_SEM_DANGER, "确定", on_proceed);

    return root;
}

void screen_confirm_show(const char *title, void (*proceed)(void))
{
    screen_confirm_show2("=PRTS二次确认=", title ? title : "确认操作?", proceed, NULL);
}

void screen_confirm_show2(const char *head, const char *desc,
                          void (*proceed)(void), void (*cancel)(void))
{
    lv_strlcpy(self.h, head ? head : "=PRTS二次确认=", sizeof(self.h));
    lv_strlcpy(self.t, desc ? desc : "确认操作?", sizeof(self.t));
    self.on_proceed = proceed;
    self.on_cancel = cancel;
    if (self.head) lv_label_set_text(self.head, self.h);
    if (self.title) lv_label_set_text(self.title, self.t);
    screen_show(SCREEN_CONFIRM);
}
