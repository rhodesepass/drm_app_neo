#include "styles.h"
#include "ui/font_registry.h"
#include "ui_metrics.h"

// 字号以 360 基准书写 (与 EEZ 烘焙字号对齐)，font_get 内部套 S()。
#define PX_LABEL_LARGE 24
#define PX_LABEL_SMALL 14
#define PX_FA_LABEL    70

// ---- label_large: 标题 ----
static lv_style_t *style_label_large(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_text_font(s, font_get(FONT_TITLE, PX_LABEL_LARGE));
    }
    return s;
}
void add_style_label_large(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_label_large(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ---- label_small: 正文 ----
static lv_style_t *style_label_small(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_text_font(s, font_get(FONT_BODY, PX_LABEL_SMALL));
    }
    return s;
}
void add_style_label_small(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_label_small(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ---- fa_label: 图标 ----
static lv_style_t *style_fa_label(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_text_font(s, font_get(FONT_ICON, PX_FA_LABEL));
    }
    return s;
}
void add_style_fa_label(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_fa_label(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ---- main_btn: 主菜单大按钮 ----
static lv_style_t *style_main_btn_default(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_text_align(s, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_color(s, lv_color_hex(0xff20679f));
    }
    return s;
}
static lv_style_t *style_main_btn_focused(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_bg_color(s, lv_color_hex(0xff398ed0));
    }
    return s;
}
void add_style_main_btn(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_main_btn_default(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, style_main_btn_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
}

// ---- main_small_btn: 重启 / 关机 ----
static lv_style_t *style_main_small_btn_default(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_bg_color(s, lv_color_hex(0xff5f1010));
    }
    return s;
}
static lv_style_t *style_main_small_btn_focused(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_bg_color(s, lv_color_hex(0xffa63737));
    }
    return s;
}
void add_style_main_small_btn(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_main_small_btn_default(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, style_main_small_btn_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
}

// ---- op_btn: 列表条目按钮 (干员/应用) ----
static lv_style_t *style_op_btn_default(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_pad_all(s, S(8));
        lv_style_set_margin_top(s, 0);
        lv_style_set_bg_color(s, lv_color_hex(0xff494947));
    }
    return s;
}
static lv_style_t *style_op_btn_focused(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_bg_color(s, lv_color_hex(0xff67d9ec));
    }
    return s;
}
void add_style_op_btn(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_op_btn_default(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, style_op_btn_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
}

// ---- op_entry: 列表条目外层间距 ----
static lv_style_t *style_op_entry(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_margin_top(s, S(5));
    }
    return s;
}
void add_style_op_entry(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_op_entry(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ---- sd_flag / app 角标 (共用底, 仅底色不同) ----
static void init_flag_base(lv_style_t *s)
{
    lv_style_set_bg_opa(s, 255);
    lv_style_set_pad_top(s, 0);
    lv_style_set_pad_bottom(s, 0);
    lv_style_set_pad_left(s, S(2));
    lv_style_set_pad_right(s, S(2));
    lv_style_set_radius(s, S(15));
    lv_style_set_text_font(s, font_get(FONT_BODY, 14));
}
static lv_style_t *flag_style(uint32_t bg)
{
    lv_style_t *s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
    lv_style_init(s);
    init_flag_base(s);
    lv_style_set_bg_color(s, lv_color_hex(bg));
    return s;
}
void add_style_sd_flag(lv_obj_t *obj)
{
    static lv_style_t *s; if (!s) s = flag_style(0xff2c3cbd);
    lv_obj_add_style(obj, s, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_bg_running(lv_obj_t *obj)
{
    static lv_style_t *s; if (!s) s = flag_style(0xff23910a);
    lv_obj_add_style(obj, s, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_fg(lv_obj_t *obj)
{
    static lv_style_t *s; if (!s) s = flag_style(0xffb3550c);
    lv_obj_add_style(obj, s, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_bg_notrunning(lv_obj_t *obj)
{
    static lv_style_t *s; if (!s) s = flag_style(0xff919197);
    lv_obj_add_style(obj, s, LV_PART_MAIN | LV_STATE_DEFAULT);
}
