#include "screen_applist.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"

static void on_back(lv_event_t *e) { (void)e; screen_show(SCREEN_MAINMENU); }

static void add_app_entry(lv_obj_t *list, const ui_app_entry_t *e)
{
    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_width(item, lv_pct(97));
    lv_obj_set_height(item, S(80));
    lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    add_style_op_entry(item);

    lv_obj_t *btn = lv_button_create(item);
    lv_obj_set_size(btn, lv_pct(100), lv_pct(100));
    add_style_op_btn(btn);

    lv_obj_t *logo = lv_image_create(btn);
    lv_obj_set_pos(logo, 0, 0);
    lv_obj_set_size(logo, S(64), S(64));
    if (e->logo_path) {
        lv_image_set_src(logo, e->logo_path);
        lv_image_set_inner_align(logo, LV_IMAGE_ALIGN_STRETCH);
    }

    lv_obj_t *name = lv_label_create(btn);
    lv_obj_set_pos(name, S(68), 0);
    lv_obj_set_width(name, S(266));
    lv_label_set_long_mode(name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    add_style_label_large(name);
    lv_label_set_text(name, e->name);

    lv_obj_t *desc = lv_label_create(btn);
    lv_obj_set_pos(desc, S(68), S(32));
    lv_obj_set_width(desc, S(245));
    add_style_label_small(desc);
    lv_label_set_text(desc, e->desc);

    // 运行状态角标
    if (e->state != UI_APP_STOPPED) {
        lv_obj_t *st = lv_label_create(btn);
        lv_obj_set_pos(st, S(303), S(47));
        if (e->state == UI_APP_BG) add_style_app_bg_running(st);
        else                       add_style_app_fg(st);
        lv_label_set_text(st, e->state == UI_APP_BG ? "后台" : "前台");
    }
    if (e->sd) {
        lv_obj_t *sd = lv_label_create(btn);
        lv_obj_set_pos(sd, S(313), S(30));
        add_style_sd_flag(sd);
        lv_label_set_text(sd, "SD");
    }
}

lv_obj_t *screen_applist_create(void)
{
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "应用列表");

    int n = ui_backend_applist_count();

    lv_obj_t *list = lv_list_create(root);
    lv_obj_set_pos(list, 0, S(40));
    lv_obj_set_size(list, S(360), S(520));
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_pad_left(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    for (int i = 0; i < n; i++) {
        ui_app_entry_t e;
        if (ui_backend_applist_get(i, &e)) add_app_entry(list, &e);
    }

    if (n == 0) {
        lv_obj_t *empty = lv_label_create(root);
        lv_obj_set_pos(empty, S(67), S(203));
        add_style_label_large(empty);
        lv_label_set_text(empty, "设备内没有应用程序\n请将程序安装到\n/app");
    }

    ui_text_button(root, 23, 574, 316, 51, 0, "返回", on_back);
    return root;
}
