#include "ui/ui_theme/theme_default.h"

// 内置默认样式表。只给中性基线外观，不压制焦点外框、不覆盖子 label 字体；
// 高级主题在 src/ui/ui_theme/ 下各自定义 (见 theme_old.c)。
#define DEFAULT_BTN_STATE { \
    .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT, \
    .border = UI_C_PRIMARY, .shadow = UI_C_COUNT, \
    .radius = 0, .pad_all = 0, .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, \
    .shadow_opa = LV_OPA_COVER, .border_width = 0, .border_side = LV_BORDER_SIDE_NONE, \
    .border_opa = LV_OPA_TRANSP, .border_post = false }

const ui_class_def_t ui_theme_classes_default[] = {
    [UI_CLS_BTN_CONFIRM] = { .name = "btn_confirm", .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                             .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0,
                             .label_text = "确定" },
    [UI_CLS_BTN_CANCEL]  = { .name = "btn_cancel",  .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                             .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0,
                             .label_text = "取消" },
    [UI_CLS_BTN_SELECT]  = { .name = "btn_select",  .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                             .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0 },
    [UI_CLS_MAIN_GRID]   = { .name = "main_grid",   .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                             .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0 },
    [UI_CLS_OPLIST_ENTRY] = {
        .name = "oplist_entry",
        .def = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
                 .border = UI_C_ACCENT, .radius = 0, .pad_all = 8,
                 .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_TRANSP },
        .foc = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
                 .border = UI_C_ACCENT, .radius = 0, .pad_all = 8,
                 .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_COVER },
        .edit = { .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
                 .border = UI_C_EDIT, .radius = 0, .pad_all = 8,
                 .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT, .border_opa = LV_OPA_COVER },
        .no_focus_ring = true,
        .focus_anim = true,
    },
    [UI_CLS_OPLIST_FLAG_RES] = {
        .name = "oplist_flag_res",
        .def = { .bg = UI_C_INFO, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
                 .radius = 15, .pad_top = 5, .pad_bottom = 1, .pad_left = 2, .pad_right = 2 },
        .no_focus_ring = true,
    },
    [UI_CLS_OPLIST_FLAG_SD] = {
        .name = "oplist_flag_sd",
        .def = { .bg = UI_C_INFO, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
                 .radius = 15, .pad_top = 5, .pad_bottom = 1, .pad_left = 2, .pad_right = 2 },
        .no_focus_ring = true,
    },
    [UI_CLS_BTN_ACTION] = {
        .name = "btn_action",
        .def = { .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
                 .radius = 0, .shadow_width = 8, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50 },
        .foc = { .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
                 .radius = 0, .shadow_width = 8, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50 },
        .no_focus_ring = false,
    },
    [UI_CLS_ACTION_RESTART]  = { .name = "action_restart",  .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                                 .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0 },
    [UI_CLS_ACTION_SHUTDOWN] = { .name = "action_shutdown", .def = DEFAULT_BTN_STATE, .foc = DEFAULT_BTN_STATE,
                                 .no_focus_ring = false, .label_font_role = FONT_BODY, .label_font_px = 0 },
};
#undef DEFAULT_BTN_STATE
