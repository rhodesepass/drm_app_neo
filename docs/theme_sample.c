//
// theme_sample —— 一套主题的「从零」参考模板（文档用，不入库注册）。
//
// 配套 docs/theme_system.md 的示例：完整展示一套主题需要的全部内容
// （类表 + 主菜单图标 + 宫格布局 + preset 色板），照抄即可开工。
// 它属于文档的一部分，放在 docs/ 下仅供阅读，不会被 CMake 编译。
//
// 要真正启用：
//   1) 复制本文件到 src/ui/ui_theme/theme_sample.c，并建同名 .h：
//        #pragma once
//        #include "ui/ui_theme.h"
//        extern const ui_class_def_t  ui_theme_classes_sample[];
//        extern const ui_theme_preset_t ui_theme_preset_sample;
//   2) 在 src/ui/ui_theme/theme.h 加一行 include。
//   3) 在 src/ui/ui_theme/themes.c 注册表加一行 &ui_theme_preset_sample。
//
#include "ui/ui_theme.h"

// ==================== 主菜单图标 / 宫格布局 ====================
// 图标填 icons.h 的码点字符串（UTF-8），各入口一个。
static const char *const sample_menu_icons[UI_MENU_ICON_COUNT] = {
    [UI_MENU_ICON_OPLIST]   = "\uf2c1",  // 干员
    [UI_MENU_ICON_DISPIMG]  = "\ue541",  // 扩列图
    [UI_MENU_ICON_APPS]     = "\uf1b2",  // 应用
    [UI_MENU_ICON_FILES]    = "\uf15b",  // 文件
    [UI_MENU_ICON_SETTINGS] = "\uf013",  // 设置
    [UI_MENU_ICON_DEV]      = "\uf51f",  // 设备
};

// 主菜单宫格按钮内 图标/文字 的对齐与偏移（360 基准）。
static const ui_grid_layout_t sample_grid_layout = {
    .icon_align = LV_ALIGN_CENTER, .icon_x = 0, .icon_y = 8, .icon_color = UI_C_MUTED,
    .text_align = LV_ALIGN_CENTER, .text_x = 0, .text_y = 0, .text_color = UI_C_TEXT,
};

// ==================== 样式类表（每个 UI_CLS_* 都要有） ====================
// 颜色填角色（UI_C_*），切主题时整表随 preset 翻转；coord 一律 360 基准，S() 由 add_class 套。
// UI_C_COUNT = 不设/继承（如 UI_CLS_BTN_ACTION 的颜色走语义 fill）。
#define SAMPLE_BTN { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT, \
    .radius = 4, .pad_all = 0, .shadow_width = 0, \
    .border_width = 0, .border_side = LV_BORDER_SIDE_NONE, .border_opa = LV_OPA_TRANSP, .border_post = false }

const ui_class_def_t ui_theme_classes_sample[] = {
    // ---- confirm / usbselect ----
    [UI_CLS_BTN_CONFIRM] = {
        .name = "btn_confirm",
        .def  = { .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .foc  = { .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0, .label_text = "确定",
    },
    [UI_CLS_BTN_CANCEL] = {
        .name = "btn_cancel",
        .def  = SAMPLE_BTN,
        .foc  = SAMPLE_BTN,
        .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0, .label_text = "取消",
    },
    [UI_CLS_BTN_SELECT] = {
        .name = "btn_select",
        .def  = SAMPLE_BTN,
        .foc  = { .bg = UI_C_ACCENT, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .no_focus_ring = false, .label_font_role = FONT_TITLE, .label_font_px = 0,
    },
    // ---- mainmenu ----
    [UI_CLS_MAIN_GRID] = {
        .name = "main_grid",
        .def  = SAMPLE_BTN,
        .foc  = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT, .radius = 4, .pad_all = 0,
                  .translate_y = -4 },
        .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0,
        .focus_anim = true, .grid_layout = &sample_grid_layout,
    },
    [UI_CLS_ACTION_RESTART] = {
        .name = "action_restart",
        .def  = { .bg = UI_C_WARNING, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .foc  = { .bg = UI_C_WARNING, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .no_focus_ring = false, .label_font_role = FONT_TITLE, .label_font_px = 0,
    },
    [UI_CLS_ACTION_SHUTDOWN] = {
        .name = "action_shutdown",
        .def  = { .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .foc  = { .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 4, .pad_all = 0 },
        .no_focus_ring = false, .label_font_role = FONT_TITLE, .label_font_px = 0,
    },
    // ---- oplist / applist 列表条目与角标 ----
    [UI_CLS_OPLIST_ENTRY] = {
        .name = "oplist_entry",
        .def  = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT, .radius = 4, .pad_all = 8,
                  .border_width = 8, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_TRANSP },
        .foc  = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT, .radius = 4, .pad_all = 8,
                  .border_width = 8, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_COVER },
        .edit = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT, .radius = 4, .pad_all = 8,
                  .border_width = 8, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_COVER },
        .no_focus_ring = true, .label_font_role = FONT_BODY, .label_font_px = 0, .focus_anim = true,
    },
    [UI_CLS_OPLIST_FLAG_RES] = {
        .name = "oplist_flag_res",
        .def  = { .bg = UI_C_FLAG_RES_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_FLAG_RES_TEXT, .radius = 8,
                  .pad_top = 3, .pad_bottom = 2, .pad_left = 8, .pad_right = 8 },
        .no_focus_ring = true, .label_font_role = FONT_BODY, .label_font_px = 0,
    },
    [UI_CLS_OPLIST_FLAG_SD] = {
        .name = "oplist_flag_sd",
        .def  = { .bg = UI_C_FLAG_SD_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, .radius = 8,
                  .pad_top = 3, .pad_bottom = 2, .pad_left = 8, .pad_right = 8 },
        .no_focus_ring = true, .label_font_role = FONT_BODY, .label_font_px = 0,
    },
    // ---- 底部动作按钮（形状由类定，颜色走语义 fill） ----
    [UI_CLS_BTN_ACTION] = {
        .name = "btn_action",
        .def  = { .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT, .radius = 4,
                  .shadow_width = 8, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50 },
        .foc  = { .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT, .radius = 4,
                  .shadow_width = 8, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50 },
        .no_focus_ring = false,
    },
};
#undef SAMPLE_BTN

// ==================== preset（色表要覆盖全部 UI_C_* 角色） ====================
// dark 决定 LVGL 基础主题深浅（开关/下拉/滚动条等标准控件随它走）。
const ui_theme_preset_t ui_theme_preset_sample = {
    .name = "Sample",
    .dark = false,
    .advanced = true,
    .cls = ui_theme_classes_sample,
    .menu_icons = sample_menu_icons,
    .ofs = NULL,          // 全用屏内基准坐标（旧版布局），需要时再给 ofs 表
    .decorate = NULL,     // 用默认页头 ui_header
    .header = NULL,
    .gauge = NULL,        // 存储仪表用默认 arc
    .titles = NULL,       // 各屏标题用屏内默认
    .size_label_dx = 0, .size_label_dy = 0,
    // 浅色底 + 蓝强调的示例色板
    .pal = {
        [UI_C_PRIMARY]=0x2f7fd0, [UI_C_PRIMARY_FOCUS]=0x4a97e0,
        [UI_C_WARNING]=0xe0a42e,
        [UI_C_DANGER]=0xc0392b,  [UI_C_DANGER_FOCUS]=0xcf4d3f,
        [UI_C_SUCCESS]=0x2e9e5b,
        [UI_C_ACCENT]=0x2f7fd0,  [UI_C_ACCENT2]=0xe0a42e,
        [UI_C_EDIT]=0xF07000,
        [UI_C_NEUTRAL]=0xf0f0f0, [UI_C_MUTED]=0x9a9a9a, [UI_C_SURFACE]=0xffffff,
        [UI_C_INFO]=0x2f7fd0,
        [UI_C_ON_ACCENT]=0xffffff,
        [UI_C_BG]=0xe8e8e8,      [UI_C_TEXT]=0x222222,  [UI_C_HEADER_TITLE]=0x222222,
        [UI_C_STRIPE1]=0x2f7fd0, [UI_C_STRIPE2]=0x4a97e0, [UI_C_STRIPE3]=0xe0a42e,
        [UI_C_SLIDER_TRACK]=0xc8c8c8, [UI_C_SLIDER_INDICATOR]=0x2f7fd0, [UI_C_SLIDER_KNOB]=0xffffff,
        [UI_C_GAUGE_TRACK]=0xc8c8c8,  [UI_C_GAUGE_INDICATOR]=0x2f7fd0,
        [UI_C_FLAG_RES_BG]=0xe0a42e,  [UI_C_FLAG_RES_TEXT]=0xffffff,
        [UI_C_FLAG_SD_BG]=0x2f7fd0,
        [UI_C_BTN_LIGHT_BG]=0xffffff, [UI_C_BTN_LIGHT_TEXT]=0x222222,
        [UI_C_IMG_OUTLINE]=0xffffff,
    },
};
