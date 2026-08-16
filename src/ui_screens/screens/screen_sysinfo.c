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

// 存储仪表由主题构建 (arc 或 bar, 见 ui_theme_gauge)；tick 按控件类型走对应 set/get。
static int gauge_value(lv_obj_t *g)
{
    if (lv_obj_check_type(g, &lv_arc_class)) return lv_arc_get_value(g);
    if (lv_obj_check_type(g, &lv_bar_class)) return lv_bar_get_value(g);
    return -1;
}
static void gauge_set_value(lv_obj_t *g, int v)
{
    if (lv_obj_check_type(g, &lv_arc_class)) lv_arc_set_value(g, v);
    else if (lv_obj_check_type(g, &lv_bar_class)) lv_bar_set_value(g, v, LV_ANIM_OFF);
}

lv_obj_t *screen_sysinfo_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root();
    ui_theme_header(root, ui_theme_title(SCREEN_SYSINFO, "设备信息"));

    // 仪表 (arc/bar) 由主题构建并定位 (sysinfo 槽位在主题钩子内)
    self.arc_nand = ui_theme_gauge(root, 0, 40, 50, 125, 125, &self.pct_nand);
    self.arc_sd   = ui_theme_gauge(root, 1, 195, 50, 127, 125, &self.pct_sd);

    lv_obj_t *t1 = lv_label_create(root);
    ui_place(t1, 52, 180, UI_OF_SLOT_SYS_LBL_NAND); add_style_label_large(t1); lv_label_set_text(t1, "系统盘");
    lv_obj_t *t2 = lv_label_create(root);
    ui_place(t2, 229, 180, UI_OF_SLOT_SYS_LBL_SD); add_style_label_large(t2); lv_label_set_text(t2, "数据盘");

    self.lbl_nand = lv_label_create(root);
    ui_place(self.lbl_nand, 35, 210, UI_OF_SLOT_SYS_PCT_NAND); lv_obj_set_width(self.lbl_nand, S(134));
    add_style_label_small(self.lbl_nand);
    lv_obj_set_style_text_align(self.lbl_nand, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.lbl_nand, "");

    self.lbl_sd = lv_label_create(root);
    ui_place(self.lbl_sd, 190, 210, UI_OF_SLOT_SYS_PCT_SD); lv_obj_set_width(self.lbl_sd, S(136));
    add_style_label_small(self.lbl_sd);
    lv_obj_set_style_text_align(self.lbl_sd, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.lbl_sd, "");

    self.info = lv_label_create(root);
    ui_place(self.info, 12, 235, UI_OF_SLOT_SYS_INFO); lv_obj_set_size(self.info, S(335), S(330));
    lv_obj_set_scrollbar_mode(self.info, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(self.info, LV_DIR_VER);
    add_style_label_small(self.info);
    lv_label_set_text(self.info, "");

    lv_obj_t *b1 = ui_text_button(root, 27, 581, 149, 51, UI_SEM_DEFAULT, "返回", on_back);
    add_class(b1, UI_CLS_BTN_ACTION);
    ui_place(b1, 27, 581, UI_OF_SLOT_SYS_BTN_BACK);
    lv_obj_t *b2 = ui_text_button(root, 186, 580, 147, 52, UI_SEM_DANGER, "格式化数据盘", on_format);
    add_class(b2, UI_CLS_BTN_ACTION);
    ui_place(b2, 186, 580, UI_OF_SLOT_SYS_BTN_FORMAT);

    screen_sysinfo_tick();
    return root;
}

void screen_sysinfo_tick(void)
{
    char buf[8];
    int n = ui_backend_nand_percent(), s = ui_backend_sd_percent();
    if (gauge_value(self.arc_nand) != n) {
        gauge_set_value(self.arc_nand, n);
        lv_snprintf(buf, sizeof(buf), "%d%%", n); lv_label_set_text(self.pct_nand, buf);
    }
    if (gauge_value(self.arc_sd) != s) {
        gauge_set_value(self.arc_sd, s);
        lv_snprintf(buf, sizeof(buf), "%d%%", s); lv_label_set_text(self.pct_sd, buf);
    }
    const char *v;
    v = ui_backend_nand_label();   if (strcmp(v, lv_label_get_text(self.lbl_nand)) != 0) lv_label_set_text(self.lbl_nand, v);
    v = ui_backend_sd_label();     if (strcmp(v, lv_label_get_text(self.lbl_sd)) != 0)   lv_label_set_text(self.lbl_sd, v);
    v = ui_backend_sysinfo_text(); if (strcmp(v, lv_label_get_text(self.info)) != 0)     lv_label_set_text(self.info, v);
}
