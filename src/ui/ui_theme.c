#include "ui/ui_theme/theme.h"
#include "font_registry.h"
#include "ui_screens/styles.h"
#include "ui_screens/screens/screen_common.h"
#include "ui_metrics.h"   // S() 缩放宏

// 主题切换引擎 —— 只负责切换 / 查询，不持有任何具体 preset / 类表定义。
// preset 与类表定义都在 src/ui/ui_theme/ 下的主题文件里，注册表在 themes.c。

static int g_id = 0;

int         ui_theme_count(void)   { return ui_theme_preset_count; }
const char *ui_theme_name(int id)  { return (id >= 0 && id < ui_theme_preset_count) ? ui_theme_presets[id]->name : ""; }
int         ui_theme_current(void) { return g_id; }
bool        ui_theme_is_dark(void) { return ui_theme_presets[g_id]->dark; }

lv_color_t ui_color(ui_color_role_t r)
{
    if (r < 0 || r >= UI_C_COUNT) return lv_color_hex(0xff00ff);
    return lv_color_hex(ui_theme_presets[g_id]->pal[r]);
}

const ui_class_def_t *ui_theme_class(ui_class_t cls)
{
    if (cls < 0 || cls >= UI_CLS_COUNT) cls = UI_CLS_BTN_SELECT;
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    const ui_class_def_t *tbl = p->advanced ? p->cls : ui_theme_classes_default;
    if (!tbl) tbl = ui_theme_classes_default;
    return &tbl[cls];
}

const char *ui_theme_menu_icon(ui_menu_icon_t icon)
{
    if (icon < 0 || icon >= UI_MENU_ICON_COUNT) return "";
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (!p->menu_icons || !p->menu_icons[icon]) return "";
    return p->menu_icons[icon];
}

void ui_theme_decorate(lv_obj_t *root, const char *title)
{
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (p->decorate) p->decorate(root, title);
    else ui_header(root, title);
}

lv_obj_t *ui_theme_header(lv_obj_t *root, const char *title)
{
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (p->header) return p->header(root, title);
    return ui_header(root, title);
}

const char *ui_theme_title(int screen_id, const char *def)
{
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (p->titles && screen_id >= 0 && p->titles[screen_id]) return p->titles[screen_id];
    return def;
}

const ui_ofs_t *ui_theme_ofs(ui_of_slot_t slot)
{
    static const ui_ofs_t kZero = {0, 0, 0, 0};
    if (slot < 0 || slot >= UI_OF_SLOT_COUNT) return &kZero;
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (!p->ofs) return &kZero;
    const ui_ofs_t *o = &p->ofs[slot];
    return (o->dx || o->dy || o->w || o->h) ? o : &kZero;
}

// 默认存储仪表：不可拖动的 arc + 中心百分比 label。
static lv_obj_t *gauge_arc_default(lv_obj_t *root, int slot,
        lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_obj_t **pct_out)
{
    (void)slot;
    lv_obj_t *arc = lv_arc_create(root);
    lv_obj_set_pos(arc, S(x), S(y));
    lv_obj_set_size(arc, S(w), S(h));
    lv_arc_set_range(arc, 0, 100);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    add_style_gauge(arc);
    // s_gauge_track 的 bg_opa=COVER 是给 bar 的轨道底色用的；对 arc 会把整个控件
    // 填成实心矩形背景。arc 只画 arc_color 的轨道环，背景要置透明。
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *pct = lv_label_create(arc);
    add_style_label_large(pct);
    lv_obj_center(pct);
    lv_label_set_text(pct, "");
    *pct_out = pct;
    return arc;
}

// 内置默认 bar 仪表：preset 可 .gauge = ui_theme_gauge_bar 直接改用条形。
lv_obj_t *ui_theme_gauge_bar(lv_obj_t *root, int slot,
        lv_coord_t base_x, lv_coord_t base_y, lv_coord_t base_w, lv_coord_t base_h,
        lv_obj_t **pct_out)
{
    (void)slot;
    lv_obj_t *bar = lv_bar_create(root);
    lv_obj_set_pos(bar, S(base_x), S(base_y));
    lv_obj_set_size(bar, S(base_w), S(base_h));
    lv_bar_set_range(bar, 0, 100);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    add_style_gauge(bar);
    lv_obj_t *pct = lv_label_create(bar);
    add_style_label_large(pct);
    lv_obj_center(pct);
    lv_label_set_text(pct, "");
    *pct_out = pct;
    return bar;
}

lv_obj_t *ui_theme_gauge(lv_obj_t *root, int slot,
        lv_coord_t base_x, lv_coord_t base_y, lv_coord_t base_w, lv_coord_t base_h,
        lv_obj_t **pct_out)
{
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (p->gauge) return p->gauge(root, slot, base_x, base_y, base_w, base_h, pct_out);
    return gauge_arc_default(root, slot, base_x, base_y, base_w, base_h, pct_out);
}

void ui_theme_size_label_ofs(int *dx, int *dy)
{
    const ui_theme_preset_t *p = ui_theme_presets[g_id];
    if (dx) *dx = p->size_label_dx;
    if (dy) *dy = p->size_label_dy;
}

void ui_theme_apply(int id)
{
    if (id < 0) id = 0;
    if (id >= ui_theme_preset_count) id = ui_theme_preset_count - 1;
    g_id = id;

    lv_display_t *disp = lv_display_get_default();
    lv_theme_t *th = lv_theme_default_init(
        disp,
        ui_color(UI_C_PRIMARY),
        ui_color(UI_C_ACCENT),
        ui_theme_presets[id]->dark,
        font_get(FONT_BODY, 14));   // 中文字体作主题默认字体 -> dropdown 展开列表等默认控件不再豆腐块
    if (disp && th) lv_display_set_theme(disp, th);

    styles_apply_palette();          // 共享 style 按新表重着色
    lv_obj_report_style_change(NULL); // 刷新全场
}

