#include "screen_oplist.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"
#include "utils/log.h"

static void on_refresh(lv_event_t *e) { (void)e; log_info("[oplist] refresh"); }
static void on_menu(lv_event_t *e)    { (void)e; screen_show(SCREEN_MAINMENU); }

// 一行干员条目：op_btn(logo + 名 + 描述 + SD 角标)
static void add_op_entry(lv_obj_t *list, const ui_op_entry_t *e)
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
    lv_obj_set_width(name, S(232));
    lv_label_set_long_mode(name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    add_style_label_large(name);
    lv_label_set_text(name, e->name);

    lv_obj_t *desc = lv_label_create(btn);
    lv_obj_set_pos(desc, S(68), S(32));
    lv_obj_set_width(desc, S(213));
    add_style_label_small(desc);
    lv_label_set_text(desc, e->desc);

    if (e->sd) {
        lv_obj_t *sd = lv_label_create(btn);
        lv_obj_set_pos(sd, S(281), S(47));
        add_style_sd_flag(sd);
        lv_label_set_text(sd, "SD");
    }
}

lv_obj_t *screen_oplist_create(void)
{
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "干员列表");

    lv_obj_t *list = lv_list_create(root);
    lv_obj_set_pos(list, S(14), S(40));
    lv_obj_set_size(list, S(332), S(280));
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_pad_left(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    int n = ui_backend_oplist_count();
    if (n == 0) {
        lv_obj_t *empty = lv_label_create(root);
        lv_obj_set_pos(empty, S(60), S(150));
        add_style_label_large(empty);
        lv_label_set_text(empty, "暂无干员素材\n请放入 /assets");
    } else {
        for (int i = 0; i < n; i++) {
            ui_op_entry_t e;
            if (ui_backend_oplist_get(i, &e)) add_op_entry(list, &e);
        }
    }

    ui_text_button(root, 17, 327, 159, 51, 0xff149b5b, "刷新列表", on_refresh);
    ui_text_button(root, 187, 327, 157, 51, 0, "主菜单", on_menu);
    return root;
}
