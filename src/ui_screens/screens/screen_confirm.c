#include "screen_confirm.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"
#include "icons.h"

static struct {
    lv_obj_t *title;
    char t[64];
    void (*on_proceed)(void);
} self;

static void on_cancel(lv_event_t *e)  { (void)e; screen_show(SCREEN_MAINMENU); }
static void on_proceed(lv_event_t *e)
{
    (void)e;
    void (*cb)(void) = self.on_proceed;
    screen_show(SCREEN_MAINMENU);
    if (cb) cb();
}

lv_obj_t *screen_confirm_create(void)
{
    lv_obj_t *root = ui_screen_root();
    lv_obj_set_style_bg_color(root, lv_color_hex(0xff9b861f), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *icon = lv_label_create(root);
    lv_obj_set_pos(icon, S(14), S(4)); add_style_fa_label(icon);
    lv_label_set_text(icon, UI_ICON_TRIANGLE_EXCLAMATION);

    lv_obj_t *head = lv_label_create(root);
    lv_obj_set_pos(head, S(83), S(4)); add_style_label_large(head);
    lv_label_set_text(head, "=PRTS二次确认=");

    self.title = lv_label_create(root);
    lv_obj_set_pos(self.title, S(83), S(37)); lv_obj_set_width(self.title, S(262));
    add_style_label_large(self.title);
    lv_label_set_text(self.title, self.t);

    ui_text_button(root, 28, 70, 149, 51, 0xff6c6666, "取消", on_cancel);
    ui_text_button(root, 187, 69, 147, 52, 0xffb10a0a, "确定", on_proceed);

    return root;
}

void screen_confirm_show(const char *title, void (*proceed)(void))
{
    lv_strlcpy(self.t, title ? title : "确认操作?", sizeof(self.t));
    self.on_proceed = proceed;
    if (self.title) lv_label_set_text(self.title, self.t);
    screen_show(SCREEN_CONFIRM);
}
