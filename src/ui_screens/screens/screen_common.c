#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"

lv_obj_t *ui_screen_root_bare(void)
{
    lv_obj_t *root = lv_obj_create(NULL);
    lv_obj_set_size(root, S(UI_BASE_WIDTH), S(UI_BASE_HEIGHT));
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return root;
}

lv_obj_t *ui_screen_root(void)
{
    lv_obj_t *root = ui_screen_root_bare();
    lv_obj_add_event_cb(root, ui_autofocus_cb, LV_EVENT_ALL, NULL);
    return root;
}

void ui_header(lv_obj_t *root, const char *title)
{
    lv_obj_t *logo = lv_image_create(root);
    lv_obj_set_pos(logo, S(15), S(10));
    lv_image_set_src(logo, LOGO_PRTS_PATH);
    lv_image_set_pivot(logo, 0, 0);
    lv_image_set_scale(logo, 128 * UI_SCALE);

    lv_obj_t *t = lv_label_create(root);
    lv_obj_set_pos(t, S(55), S(14));
    add_style_label_large(t);
    lv_label_set_text(t, title);
}

lv_obj_t *ui_text_button(lv_obj_t *root, int x, int y, int w, int h,
                         uint32_t bg, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(root);
    lv_obj_set_pos(o, S(x), S(y));
    lv_obj_set_size(o, S(w), S(h));
    if (bg) lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (cb) lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *lbl = lv_label_create(o);
    add_style_label_large(lbl);
    lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(lbl, text);
    return o;
}

lv_obj_t *ui_small_text_button(lv_obj_t *root, int x, int y, int w, int h,
    uint32_t bg, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(root);
    lv_obj_set_pos(o, S(x), S(y));
    lv_obj_set_size(o, S(w), S(h));
    if (bg) lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (cb) lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *lbl = lv_label_create(o);
    add_style_label_small(lbl);
    lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(lbl, text);
    return o;
}

static void add_focusables(lv_obj_t *parent, lv_group_t *g)
{
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(c, &lv_button_class) ||
            lv_obj_check_type(c, &lv_dropdown_class) ||
            lv_obj_check_type(c, &lv_switch_class) ||
            lv_obj_check_type(c, &lv_slider_class) ||
            lv_obj_check_type(c, &lv_roller_class)) {
            lv_group_add_obj(g, c);
        }
        add_focusables(c, g);
    }
}

void ui_autofocus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    add_focusables(lv_event_get_target(e), g);
}
