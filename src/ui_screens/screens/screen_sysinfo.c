#include "screen_sysinfo.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "screen_confirm.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"
#include "utils/log.h"

static struct {
    lv_obj_t *arc_nand, *arc_sd;
    lv_obj_t *pct_nand, *pct_sd;
    lv_obj_t *lbl_nand, *lbl_sd;
    lv_obj_t *info;
} self;

static void on_back(lv_event_t *e)   { (void)e; screen_show(SCREEN_MAINMENU); }
static void on_format(lv_event_t *e) { (void)e; screen_confirm_show("确定格式化数据盘吗？", ui_hook_format_sd); }

// 仪表盘式 arc (不可拖动) + 中心百分比
static lv_obj_t *gauge(lv_obj_t *root, int x, int w, lv_obj_t **pct_out)
{
    lv_obj_t *arc = lv_arc_create(root);
    lv_obj_set_pos(arc, S(x), S(50));
    lv_obj_set_size(arc, S(w), S(125));
    lv_arc_set_range(arc, 0, 100);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *pct = lv_label_create(arc);
    add_style_label_large(pct);
    lv_obj_center(pct);
    lv_label_set_text(pct, "");
    *pct_out = pct;
    return arc;
}

lv_obj_t *screen_sysinfo_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "设备信息");

    self.arc_nand = gauge(root, 40, 125, &self.pct_nand);
    self.arc_sd   = gauge(root, 195, 127, &self.pct_sd);

    lv_obj_t *t1 = lv_label_create(root);
    lv_obj_set_pos(t1, S(52), S(180)); add_style_label_large(t1); lv_label_set_text(t1, "内部存储");
    lv_obj_t *t2 = lv_label_create(root);
    lv_obj_set_pos(t2, S(229), S(180)); add_style_label_large(t2); lv_label_set_text(t2, "SD卡");

    self.lbl_nand = lv_label_create(root);
    lv_obj_set_pos(self.lbl_nand, S(35), S(210)); lv_obj_set_width(self.lbl_nand, S(134));
    add_style_label_small(self.lbl_nand);
    lv_obj_set_style_text_align(self.lbl_nand, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.lbl_nand, "");

    self.lbl_sd = lv_label_create(root);
    lv_obj_set_pos(self.lbl_sd, S(190), S(210)); lv_obj_set_width(self.lbl_sd, S(136));
    add_style_label_small(self.lbl_sd);
    lv_obj_set_style_text_align(self.lbl_sd, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.lbl_sd, "");

    self.info = lv_label_create(root);
    lv_obj_set_pos(self.info, S(12), S(235)); lv_obj_set_size(self.info, S(335), S(330));
    lv_obj_set_scrollbar_mode(self.info, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(self.info, LV_DIR_VER);
    add_style_label_small(self.info);
    lv_label_set_text(self.info, "");

    ui_text_button(root, 27, 581, 149, 51, UI_SEM_DEFAULT, "返回", on_back);
    ui_text_button(root, 186, 580, 147, 52, UI_SEM_DANGER, "格式化数据盘", on_format);

    screen_sysinfo_tick();
    return root;
}

void screen_sysinfo_tick(void)
{
    char buf[8];
    int n = ui_backend_nand_percent(), s = ui_backend_sd_percent();
    if (lv_arc_get_value(self.arc_nand) != n) {
        lv_arc_set_value(self.arc_nand, n);
        lv_snprintf(buf, sizeof(buf), "%d%%", n); lv_label_set_text(self.pct_nand, buf);
    }
    if (lv_arc_get_value(self.arc_sd) != s) {
        lv_arc_set_value(self.arc_sd, s);
        lv_snprintf(buf, sizeof(buf), "%d%%", s); lv_label_set_text(self.pct_sd, buf);
    }
    const char *v;
    v = ui_backend_nand_label();   if (strcmp(v, lv_label_get_text(self.lbl_nand)) != 0) lv_label_set_text(self.lbl_nand, v);
    v = ui_backend_sd_label();     if (strcmp(v, lv_label_get_text(self.lbl_sd)) != 0)   lv_label_set_text(self.lbl_sd, v);
    v = ui_backend_sysinfo_text(); if (strcmp(v, lv_label_get_text(self.info)) != 0)     lv_label_set_text(self.info, v);
}
