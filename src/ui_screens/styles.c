#include "styles.h"
#include "ui/font_registry.h"

// 字号以 360 基准书写 (与 EEZ 烘焙字号对齐)，font_get 内部套 S()。
#define PX_LABEL_LARGE 24
#define PX_LABEL_SMALL 14
#define PX_FA_LABEL    28

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
