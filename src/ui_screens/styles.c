#include "styles.h"
#include "ui/font_registry.h"
#include "ui/ui_theme.h"
#include "ui_metrics.h"
#include "utils/log.h"

// 字号以 360 基准书写 (与 EEZ 烘焙字号对齐)，font_get 内部套 S()。
#define PX_LABEL_LARGE 24
#define PX_LABEL_SMALL 14
#define PX_FA_LABEL    60
#define PX_FA_SMALL   24

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

// ---- fa_small_label: 小图标 ----
static lv_style_t *style_fa_small_label(void)
{
    static lv_style_t *s;
    if (!s) {
        s = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(s);
        lv_style_set_text_font(s, font_get(FONT_ICON, PX_FA_SMALL));
    }
    return s;
}
void add_style_fa_small_label(lv_obj_t *obj)
{
    lv_obj_add_style(obj, style_fa_small_label(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// ======================= 带色 style (随主题翻转) =======================
// 非色属性 (pad/radius/margin/opa/font) 首次 ensure 时设一次；颜色统一在
// styles_apply_palette() 里按当前调色板 (重)设，切主题走重着色刷新，不重建屏。
static bool s_inited;

static lv_style_t s_op_btn_def,     s_op_btn_foc;
static lv_style_t s_op_entry;       // 仅 margin，无色
static lv_style_t s_flag_sd, s_flag_run, s_flag_fg, s_flag_notrun, s_flag_res;
static lv_style_t s_flag_compact; // 紧凑角标 (oplist 沿用: 直角/对称pad/阴影)
static lv_style_t s_fill_primary, s_fill_warning, s_fill_danger, s_fill_success, s_fill_neutral;
static lv_style_t s_fill_light; // 浅色底按钮 (oplist 刷新列表)
static lv_style_t s_spinner_arc, s_log_text;
static lv_style_t s_slider_track, s_slider_indicator, s_slider_knob; // 亮度滑条 (轨道/已填充/旋钮)
static lv_style_t s_gauge_track, s_gauge_ind;                        // 存储仪表 (轨道/已填充)
static lv_style_t s_switch_track, s_switch_ind, s_switch_knob;       // 开关 (轨道/填充/旋钮)
static lv_style_t s_dd_main, s_dd_list, s_dd_sel;                    // 下拉 (主框/展开列表/选中项)
static lv_style_t s_focus;      // 键盘焦点外框 (比主题默认更粗、随 S() 缩放)
static lv_style_t s_screen_bg;  // 屏幕背景 (随方案换底)
static lv_style_t s_theme_stripe; // 主题风格条 (单条, 主题循环建 3 条, 随主题翻转颜色)
static lv_style_t s_cls_def[UI_CLS_COUNT]; // 样式类 (CSS class): 每个类的默认/聚焦态
static lv_style_t s_cls_foc[UI_CLS_COUNT];
static lv_style_t s_cls_edit[UI_CLS_COUNT]; // 样式类编辑/排序态 (LV_STATE_USER_1)

static void init_flag_base(lv_style_t *s)
{
    lv_style_init(s);
    lv_style_set_bg_opa(s, 255);
    // 上下留白，让彩色圆角底完整包住整行文字 (含中文字形的上下伸展)，
    // 否则贴字太紧会露出字的顶/底。调整时注意 oplist/applist 里两个竖直堆叠的
    // 角标间距 (见各屏 make_slot 的 y 坐标)，别让加高后的角标互相重叠。
    // 上多下少：字体行盒底部含 descender 空隙，数字/中文会偏上，多给顶距把字压低居中。
    lv_style_set_pad_top(s, S(5));
    lv_style_set_pad_bottom(s, S(1));
    lv_style_set_pad_left(s, S(2));
    lv_style_set_pad_right(s, S(2));
    lv_style_set_radius(s, S(15));
    lv_style_set_text_font(s, font_get(FONT_BODY, 14));
    // 角标一律彩色饱和底，配白字保证可读 (不随主题继承按钮文字色)。
    lv_style_set_text_color(s, lv_color_white());
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

    // 紧凑角标：直角、对称上下 pad、16px 水平 pad、阴影
    lv_style_init(&s_flag_compact);
    lv_style_set_bg_opa(&s_flag_compact, 255);
    lv_style_set_radius(&s_flag_compact, 0);
    lv_style_set_pad_top(&s_flag_compact, 3);
    lv_style_set_pad_bottom(&s_flag_compact, 2);
    lv_style_set_pad_left(&s_flag_compact, 8);
    lv_style_set_pad_right(&s_flag_compact, 16);
    lv_style_set_text_font(&s_flag_compact, font_get(FONT_BODY, 14));
    lv_style_set_shadow_width(&s_flag_compact, 8);
    lv_style_set_shadow_offset_y(&s_flag_compact, 2);

    // 主题风格条：3px 高细条、无边框、无滚动
    lv_style_init(&s_theme_stripe);
    lv_style_set_bg_opa(&s_theme_stripe, 255);
    lv_style_set_radius(&s_theme_stripe, 0);
    lv_style_set_border_width(&s_theme_stripe, 0);

    // 样式类：默认/聚焦/编辑三态各一个共享 style，属性在 styles_apply_palette() 里整表重设
    for (int i = 0; i < UI_CLS_COUNT; i++) {
        lv_style_init(&s_cls_def[i]);
        lv_style_init(&s_cls_foc[i]);
        lv_style_init(&s_cls_edit[i]);
    }

    init_fill(&s_fill_primary);
    init_fill(&s_fill_warning);
    init_fill(&s_fill_danger);
    init_fill(&s_fill_success);
    init_fill(&s_fill_neutral);
    init_fill(&s_fill_light);

    lv_style_init(&s_spinner_arc);
    lv_style_init(&s_log_text);

    // 亮度滑条：直角轨道/旋钮 (颜色在 palette 里设)
    lv_style_init(&s_slider_track);
    lv_style_set_radius(&s_slider_track, 0);
    lv_style_set_bg_opa(&s_slider_track, 200);
    lv_style_init(&s_slider_indicator);
    lv_style_set_radius(&s_slider_indicator, 0);
    lv_style_init(&s_slider_knob);
    lv_style_set_radius(&s_slider_knob, 0);

    // 存储仪表 (arc/bar)：轨道/已填充 (颜色在 palette 里设)
    lv_style_init(&s_gauge_track);
    lv_style_set_bg_opa(&s_gauge_track, LV_OPA_COVER);
    lv_style_set_arc_opa(&s_gauge_track, LV_OPA_COVER);
    lv_style_init(&s_gauge_ind);
    lv_style_set_bg_opa(&s_gauge_ind, LV_OPA_COVER);
    lv_style_set_arc_opa(&s_gauge_ind, LV_OPA_COVER);

    // 开关：圆角轨道/指示器/旋钮 (颜色在 palette 里设, 复用滑条角色)
    lv_style_init(&s_switch_track);
    lv_style_set_radius(&s_switch_track, S(15));
    lv_style_set_bg_opa(&s_switch_track, LV_OPA_COVER);
    lv_style_init(&s_switch_ind);
    lv_style_set_radius(&s_switch_ind, S(15));
    lv_style_set_bg_opa(&s_switch_ind, LV_OPA_COVER);
    lv_style_init(&s_switch_knob);
    lv_style_set_radius(&s_switch_knob, S(15));
    lv_style_set_bg_opa(&s_switch_knob, LV_OPA_COVER);

    // 下拉：主框底/文字/边框 + 展开列表/选中项 (颜色在 palette 里设)
    lv_style_init(&s_dd_main);
    lv_style_set_bg_opa(&s_dd_main, LV_OPA_COVER);
    lv_style_set_radius(&s_dd_main, S(4));
    lv_style_set_border_width(&s_dd_main, S(1));
    lv_style_init(&s_dd_list);
    lv_style_set_bg_opa(&s_dd_list, LV_OPA_COVER);
    lv_style_set_radius(&s_dd_list, S(4));
    lv_style_init(&s_dd_sel);
    lv_style_set_bg_opa(&s_dd_sel, LV_OPA_COVER);

    lv_style_init(&s_focus);
    lv_style_set_outline_width(&s_focus, S(4));
    lv_style_set_outline_pad(&s_focus, S(2));
    lv_style_set_outline_opa(&s_focus, LV_OPA_COVER);

    lv_style_init(&s_screen_bg);
    lv_style_set_bg_opa(&s_screen_bg, LV_OPA_COVER);

    styles_apply_palette();
}

// 把一个类状态 (默认/聚焦) 的完整外观写进 style：颜色查 ui_color，coord 套 S()。
static void apply_class_state(lv_style_t *s, const ui_class_state_t *st)
{
    if (st->bg     != UI_C_COUNT) lv_style_set_bg_color(s, ui_color(st->bg));
    if (st->text   != UI_C_COUNT) lv_style_set_text_color(s, ui_color(st->text));
    if (st->border != UI_C_COUNT) lv_style_set_border_color(s, ui_color(st->border));
    if (st->shadow != UI_C_COUNT) lv_style_set_shadow_color(s, ui_color(st->shadow));
    lv_style_set_bg_opa(s, st->bg_opa);
    // radius<0 = 不设圆角，走 LVGL 默认主题圆角；切主题时要清掉旧值避免残留
    if (st->radius >= 0) lv_style_set_radius(s, S(st->radius));
    else                 lv_style_remove_prop(s, LV_STYLE_RADIUS);
    if (st->pad_top || st->pad_bottom || st->pad_left || st->pad_right) {
        lv_style_set_pad_top(s, S(st->pad_top));
        lv_style_set_pad_bottom(s, S(st->pad_bottom));
        lv_style_set_pad_left(s, S(st->pad_left));
        lv_style_set_pad_right(s, S(st->pad_right));
    } else {
        lv_style_set_pad_all(s, S(st->pad_all));
    }
    lv_style_set_shadow_width(s, S(st->shadow_width));
    lv_style_set_shadow_offset_x(s, S(st->shadow_ofs_x));
    lv_style_set_shadow_offset_y(s, S(st->shadow_ofs_y));
    lv_style_set_shadow_opa(s, st->shadow_opa);
    lv_style_set_border_width(s, S(st->border_width));
    lv_style_set_border_side(s, st->border_side);
    lv_style_set_border_opa(s, st->border_opa);
    lv_style_set_border_post(s, st->border_post);
    lv_style_set_translate_x(s, S(st->translate_x));
    lv_style_set_translate_y(s, S(st->translate_y));
}

// 样式类聚焦过渡：统一 150ms 缓出，动画覆盖类状态里会变的**绘制**属性。
// translate 是布局属性，不能放进动画：重建屏后按钮"创建即聚焦"，过渡动画会用
// trans_style 把 translate 锁在旧值 0、动画推进不到目标值，导致不位移。
// 布局属性走正常的 DIFF_LAYOUT -> 布局更新路径，立即生效；动画只留给绘制属性。
static const lv_style_prop_t s_cls_focus_props[] = {
    LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA,
    LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_WIDTH, LV_STYLE_BORDER_OPA,
    LV_STYLE_SHADOW_WIDTH, LV_STYLE_SHADOW_OFFSET_Y, LV_STYLE_SHADOW_OPA,
    LV_STYLE_TEXT_COLOR,
    LV_STYLE_PROP_INV,
};
static const lv_style_transition_dsc_t s_cls_focus_trans = {
    .props    = s_cls_focus_props,
    .time     = 150,
    .delay    = 0,
    .path_xcb = lv_anim_path_ease_out,
};

void styles_apply_palette(void)
{
    if (!s_inited) { styles_ensure(); return; } // ensure 末尾会回调本函数

    lv_style_set_bg_color(&s_op_btn_def,     ui_color(UI_C_NEUTRAL));
    lv_style_set_bg_color(&s_op_btn_foc,     ui_color(UI_C_ACCENT));
    // 列表条目里的 name/desc 标签不自带文字色，会继承按钮的文字色。LVGL 主题给每个按钮
    // 都叠了 bg_color_primary(primary 底 + 白字)，白字在浅色方案的浅灰中性底上几乎看不清。
    // 用 LVGL 主题标题/正文同款 color_text 覆盖 —— 与顶部 "干员列表" 标题完全一致
    // (深色: 亮浅灰 / 浅色: 深灰)，两套方案都可读。聚焦态底是 accent 高亮，保持白字。
    lv_color_t list_text = ui_theme_is_dark() ? lv_palette_lighten(LV_PALETTE_GREY, 5)
                                              : lv_palette_darken(LV_PALETTE_GREY, 4);
    lv_style_set_text_color(&s_op_btn_def, list_text);
    lv_style_set_text_color(&s_op_btn_foc, ui_color(UI_C_ON_ACCENT));

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
    lv_style_set_bg_color(&s_fill_light,   ui_color(UI_C_BTN_LIGHT_BG));
    lv_style_set_text_color(&s_fill_light, ui_color(UI_C_BTN_LIGHT_TEXT));
    // 饱和强调底一律配 on_accent(白)字，保证深/浅方案下按钮文字都可读；
    // 中性底不强制文字色，随 LVGL 主题深浅走 (灰底配灰底该有的字色)。
    lv_style_set_text_color(&s_fill_primary, ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_warning, ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_danger,  ui_color(UI_C_ON_ACCENT));
    lv_style_set_text_color(&s_fill_success, ui_color(UI_C_ON_ACCENT));

    lv_style_set_arc_color(&s_spinner_arc, ui_color(UI_C_MUTED));
    lv_style_set_text_color(&s_log_text,   ui_color(UI_C_MUTED));

    lv_style_set_bg_color(&s_slider_track,     ui_color(UI_C_SLIDER_TRACK));
    lv_style_set_bg_color(&s_slider_indicator, ui_color(UI_C_SLIDER_INDICATOR));
    lv_style_set_bg_color(&s_slider_knob,      ui_color(UI_C_SLIDER_KNOB));

    lv_style_set_bg_color(&s_gauge_track,  ui_color(UI_C_GAUGE_TRACK));
    lv_style_set_arc_color(&s_gauge_track, ui_color(UI_C_GAUGE_TRACK));
    lv_style_set_bg_color(&s_gauge_ind,    ui_color(UI_C_GAUGE_INDICATOR));
    lv_style_set_arc_color(&s_gauge_ind,   ui_color(UI_C_GAUGE_INDICATOR));

    lv_style_set_bg_color(&s_switch_track, ui_color(UI_C_SLIDER_TRACK));
    lv_style_set_bg_color(&s_switch_ind,   ui_color(UI_C_SLIDER_INDICATOR));
    lv_style_set_bg_color(&s_switch_knob,  ui_color(UI_C_SLIDER_KNOB));

    lv_style_set_bg_color(&s_dd_main,     ui_color(UI_C_SURFACE));
    lv_style_set_text_color(&s_dd_main,   ui_color(UI_C_TEXT));
    lv_style_set_border_color(&s_dd_main, ui_color(UI_C_NEUTRAL));
    lv_style_set_bg_color(&s_dd_list,     ui_color(UI_C_SURFACE));
    lv_style_set_text_color(&s_dd_list,   ui_color(UI_C_TEXT));
    lv_style_set_bg_color(&s_dd_sel,      ui_color(UI_C_PRIMARY));
    lv_style_set_text_color(&s_dd_sel,    ui_color(UI_C_ON_ACCENT));

    lv_style_set_outline_color(&s_focus, ui_color(UI_C_ACCENT));
    lv_style_set_bg_color(&s_screen_bg,  ui_color(UI_C_BG));

    lv_style_set_text_color(&s_flag_compact, lv_color_white());

    // 样式类: 按当前主题的类表整表重设 (不只颜色, 半径/阴影/边框/选中效果都随主题变)
    for (int i = 0; i < UI_CLS_COUNT; i++) {
        const ui_class_def_t *cls = ui_theme_class((ui_class_t)i);
        apply_class_state(&s_cls_def[i], &cls->def);
        apply_class_state(&s_cls_foc[i], &cls->foc);
        apply_class_state(&s_cls_edit[i], &cls->edit);
        // 聚焦过渡动画：挂在 DEFAULT 态上 (选择器 0 恒命中，进/出聚焦都平滑)
        if (cls->focus_anim) {
            lv_style_set_transition(&s_cls_def[i], &s_cls_focus_trans);
        } else {
            lv_style_remove_prop(&s_cls_def[i], LV_STYLE_TRANSITION);
        }
    }
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
void add_style_flag_compact(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_flag_compact, LV_PART_MAIN | LV_STATE_DEFAULT);
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
        case UI_SEM_LIGHT:   s = &s_fill_light;   break;
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

void add_style_main_slider(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_slider_track,     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_slider_indicator, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_slider_knob,      LV_PART_KNOB | LV_STATE_DEFAULT);
}

void add_style_gauge(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_gauge_track, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_gauge_ind,   LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

void add_style_switch(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_switch_track, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_switch_ind,   LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_switch_knob,  LV_PART_KNOB | LV_STATE_DEFAULT);
}

// 下拉展开列表懒创建：存在时直接套样式；首次展开后由值变更事件补套。
static void style_dd_list(lv_obj_t *dd)
{
    lv_obj_t *list = lv_dropdown_get_list(dd);
    if (!list) return;
    lv_obj_add_style(list, &s_dd_list, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(list, &s_dd_sel,  LV_PART_SELECTED | LV_STATE_DEFAULT);
}
static void on_dd_list_created(lv_event_t *e)
{
    style_dd_list(lv_event_get_target(e));
}
void add_style_dropdown(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_dd_main, LV_PART_MAIN | LV_STATE_DEFAULT);
    style_dd_list(obj);                          // 已展开过则直接套
    lv_obj_add_event_cb(obj, on_dd_list_created, LV_EVENT_VALUE_CHANGED, NULL);
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

void add_style_theme_stripe(lv_obj_t *obj)
{
    styles_ensure();
    lv_obj_add_style(obj, &s_theme_stripe, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// LVGL 在对象未 rendered 时跳过聚焦态的 DIFF_LAYOUT 处理 (lv_obj.c 的
// "Skip transitions if the widget is not rendered yet")，导致布局属性 (如 translate)
// 在"创建即聚焦"时首次聚焦不生效 (第二次聚焦才正常)。这里在聚焦时补一次布局脏标记，
// 让首次渲染刷新位置。不依赖具体是 translate 还是 border 等布局属性，通用生效。
static void cls_focus_layout_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_FOCUSED) return;
    lv_obj_mark_layout_as_dirty(lv_event_get_target(e));
}

void add_class(lv_obj_t *obj, ui_class_t cls)
{
    if (cls < 0 || cls >= UI_CLS_COUNT) return;
    styles_ensure();
    lv_obj_add_style(obj, &s_cls_def[cls], LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &s_cls_foc[cls], LV_PART_MAIN | LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(obj, &s_cls_edit[cls], LV_PART_MAIN | LV_STATE_USER_1);
    lv_obj_add_event_cb(obj, cls_focus_layout_cb, LV_EVENT_FOCUSED, NULL);

    const ui_class_def_t *c = ui_theme_class(cls);
    // 类级行为: 压制默认焦点外框 (add_focusables 后加的 s_focus 会被局部样式覆盖)
    if (c->no_focus_ring) {
        lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    }
    // 类级行为: 覆盖子 label 字体 (如 confirm 的图标字形)。ui_text_button 的 label 是首个 child。
    if (c->label_font_px > 0 && lv_obj_get_child_count(obj) > 0) {
        lv_obj_set_style_text_font(lv_obj_get_child(obj, 0),
                                   font_get(c->label_font_role, c->label_font_px),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    // 类级行为: 覆盖首个子 label 文本 (confirm 的 "确定/取消" / 图标码点)。由主题决定按钮内容。
    if (c->label_text && lv_obj_get_child_count(obj) > 0) {
        lv_label_set_text(lv_obj_get_child(obj, 0), c->label_text);
    }
    // 类级行为: 覆盖按钮尺寸 (w/h>0 时, 局部样式压过 ui_text_button 的 set_size)
    if (c->w > 0) lv_obj_set_width(obj, S(c->w));
    if (c->h > 0) lv_obj_set_height(obj, S(c->h));
    // 坐标平移走 ui_theme_ofs 槽位 (见 screen_common.ui_place)，不在类表里做。
    // 主题钩子: 添加/覆盖内容子节点 (如选中指示器)。
    if (c->build_content) c->build_content(obj, cls);
}
