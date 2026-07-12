#include "styles.h"
#include "ui/font_registry.h"
#include "ui/ui_theme.h"
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

void set_style_label_size(lv_obj_t *obj, bool large)
{
    lv_obj_remove_style(obj, style_label_large(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, style_label_small(), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (large)
        add_style_label_large(obj);
    else
        add_style_label_small(obj);
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

// ======================= 带色 style (随主题翻转) =======================
// 非色属性 (pad/radius/margin/opa/font) 首次 ensure 时设一次；颜色统一在
// styles_apply_palette() 里按当前调色板 (重)设，切主题走重着色刷新，不重建屏。
static bool s_inited;

static lv_style_t s_main_btn_def,   s_main_btn_foc;
static lv_style_t s_main_small_def, s_main_small_foc;
static lv_style_t s_op_btn_def,     s_op_btn_foc;
static lv_style_t s_op_entry;       // 仅 margin，无色
static lv_style_t s_flag_sd, s_flag_run, s_flag_fg, s_flag_notrun, s_flag_res;
static lv_style_t s_fill_primary, s_fill_warning, s_fill_danger, s_fill_success, s_fill_neutral;
static lv_style_t s_spinner_arc, s_log_text;
static lv_style_t s_focus;      // 键盘焦点外框 (比主题默认更粗、随 S() 缩放)
static lv_style_t s_screen_bg;  // 屏幕背景 (随方案换底)

static void init_flag_base(lv_style_t *s)
{
    lv_style_init(s);
    lv_style_set_bg_opa(s, 255);
    lv_style_set_pad_top(s, 0);
    lv_style_set_pad_bottom(s, 0);
    lv_style_set_pad_left(s, S(2));
    lv_style_set_pad_right(s, S(2));
    lv_style_set_radius(s, S(15));
    lv_style_set_text_font(s, font_get(FONT_BODY, 14));
}

static void init_fill(lv_style_t *s)
{
    lv_style_init(s);
    lv_style_set_bg_opa(s, LV_OPA_COVER);
}

static void styles_ensure(void)
{
    if (s_inited) return;
    s_inited = true;

    lv_style_init(&s_main_btn_def);
    lv_style_set_text_align(&s_main_btn_def, LV_TEXT_ALIGN_CENTER);
    lv_style_init(&s_main_btn_foc);

    lv_style_init(&s_main_small_def);
    lv_style_init(&s_main_small_foc);

    lv_style_init(&s_op_btn_def);
    lv_style_set_pad_all(&s_op_btn_def, S(8));
    lv_style_set_margin_top(&s_op_btn_def, 0);
    lv_style_init(&s_op_btn_foc);

    lv_style_init(&s_op_entry);
    lv_style_set_margin_top(&s_op_entry, S(5));

    init_flag_base(&s_flag_sd);
    init_flag_base(&s_flag_run);
    init_flag_base(&s_flag_fg);
    init_flag_base(&s_flag_notrun);
    init_flag_base(&s_flag_res);

    init_fill(&s_fill_primary);
    init_fill(&s_fill_warning);
    init_fill(&s_fill_danger);
    init_fill(&s_fill_success);
    init_fill(&s_fill_neutral);

    lv_style_init(&s_spinner_arc);
    lv_style_init(&s_log_text);

    lv_style_init(&s_focus);
    lv_style_set_outline_width(&s_focus, S(4));
    lv_style_set_outline_pad(&s_focus, S(2));
    lv_style_set_outline_opa(&s_focus, LV_OPA_COVER);

    lv_style_init(&s_screen_bg);
    lv_style_set_bg_opa(&s_screen_bg, LV_OPA_COVER);

    styles_apply_palette();
}

void styles_apply_palette(void)
{
    if (!s_inited) { styles_ensure(); return; } // ensure 末尾会回调本函数

    lv_style_set_bg_color(&s_main_btn_def,   ui_color(UI_C_PRIMARY));
    lv_style_set_bg_color(&s_main_btn_foc,   ui_color(UI_C_PRIMARY_FOCUS));
    lv_style_set_bg_color(&s_main_small_def, ui_color(UI_C_DANGER));
    lv_style_set_bg_color(&s_main_small_foc, ui_color(UI_C_DANGER_FOCUS));
    lv_style_set_bg_color(&s_op_btn_def,     ui_color(UI_C_NEUTRAL));
    lv_style_set_bg_color(&s_op_btn_foc,     ui_color(UI_C_ACCENT));

    lv_style_set_bg_color(&s_flag_sd,     ui_color(UI_C_INFO));
    lv_style_set_bg_color(&s_flag_run,    ui_color(UI_C_SUCCESS));
    lv_style_set_bg_color(&s_flag_fg,     ui_color(UI_C_WARNING));
    lv_style_set_bg_color(&s_flag_notrun, ui_color(UI_C_MUTED));
    lv_style_set_bg_color(&s_flag_res,    ui_color(UI_C_PRIMARY));

    lv_style_set_bg_color(&s_fill_primary, ui_color(UI_C_PRIMARY));
    lv_style_set_bg_color(&s_fill_warning, ui_color(UI_C_WARNING));
    lv_style_set_bg_color(&s_fill_danger,  ui_color(UI_C_DANGER));
    lv_style_set_bg_color(&s_fill_success, ui_color(UI_C_SUCCESS));
    lv_style_set_bg_color(&s_fill_neutral, ui_color(UI_C_MUTED));
    // 饱和强调底一律配 on_accent(白)字，保证深/浅方案下按钮文字都可读；
    // 中性底不强制文字色，随 LVGL 主题深浅走 (灰底配灰底该有的字色)。
    lv_style_set_text_color(&s_fill_primary, ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_warning, ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_danger,  ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_success, ui_color(UI_C_ON_ACCENT));

    lv_style_set_arc_color(&s_spinner_arc, ui_color(UI_C_MUTED));
    lv_style_set_text_color(&s_log_text,   ui_color(UI_C_MUTED));

    lv_style_set_outline_color(&s_focus, ui_color(UI_C_ACCENT));
    lv_style_set_bg_color(&s_screen_bg,  ui_color(UI_C_BG));
}

void add_style_main_btn(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_main_btn_def, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_main_btn_foc, LV_PART_MAIN | LV_STATE_FOCUSED);
}

void add_style_main_small_btn(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_main_small_def, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_main_small_foc, LV_PART_MAIN | LV_STATE_FOCUSED);
}

void add_style_op_btn(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_op_btn_def, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_op_btn_foc, LV_PART_MAIN | LV_STATE_FOCUSED);
}

void add_style_op_entry(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_op_entry, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void add_style_sd_flag(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_sd, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_res_flag(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_res, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_bg_running(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_run, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_fg(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_fg, LV_PART_MAIN | LV_STATE_DEFAULT);
}
void add_style_app_bg_notrunning(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_notrun, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void add_style_fill(lv_obj_t *obj, ui_sem_t sem)
{
    styles_ensure();
    lv_style_t *s = NULL;
    switch (sem) {
        case UI_SEM_PRIMARY: s = &s_fill_primary; break;
        case UI_SEM_WARNING: s = &s_fill_warning; break;
        case UI_SEM_DANGER:  s = &s_fill_danger;  break;
        case UI_SEM_SUCCESS: s = &s_fill_success; break;
        case UI_SEM_NEUTRAL: s = &s_fill_neutral; break;
        case UI_SEM_DEFAULT: default: return; // 走主题默认底色
    }
    lv_obj_add_style(obj, s, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void add_style_spinner_arc(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_spinner_arc, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void add_style_log_text(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_log_text, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// 焦点外框。进屏 (autofocus) 每次都会调，先 remove 再 add 保证只挂一份不叠加。
void add_style_focus(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_remove_style(obj, &s_focus, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(obj, &s_focus, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
}

void add_style_screen_bg(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_screen_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
}
