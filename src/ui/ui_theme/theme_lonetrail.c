#include "ui/ui_theme/theme_lonetrail.h"
#include "ui_screens/styles.h"
#include "ui_screens/screens/screen_common.h"
#include "icons.h"
#include "ui_metrics.h"

// Lonetrail（孤星）高级样式主题。coord 一律 360 基准，styles.c 应用时套 S()；
// 颜色角色 UI_C_COUNT = 继承主题默认。类级行为:
//   no_focus_ring  = 干掉默认焦点外框 (改由 " >" 指示器 / 宫格装饰 等主题选中效果)
//   label_font_*   = 覆盖子 label 字体 (confirm 两个按钮是图标字形)
//   build_content  = 给按钮添加选中指示器 / 装饰块 (内容子节点)
// 设计风格：confirm/usbselect 按钮用「选中指示器 + 图标字形」交互；主菜单宫格按钮
//   统一直角 + 装饰块风格 (底部三色横条 + 右上角点)，浅色奶油底 + 橙色强调。
// 本文件按「页面」分组，类表条目顺序与人读无关 (designated initializer)。

// ==================== 页面: 确认框 confirm / USB选择 usbselect ====================
//   —— 选中指示器 (btn_confirm/cancel/select 共用的 " >   <" 或 ▶)

/* 聚焦指示器: 聚焦显示 / 失焦隐藏 */
static void on_focus_indicator(lv_event_t *e)
{
    lv_obj_t *ind = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_remove_flag(ind, LV_OBJ_FLAG_HIDDEN);
    else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED)
        lv_obj_add_flag(ind, LV_OBJ_FLAG_HIDDEN);
}

// 主题给按钮类添加的选中指示器 (内容子节点)。
// btn_confirm/btn_cancel: 宽 " >   <" 居中；btn_select: 左侧小 ▶。
static void build_content_lonetrail(lv_obj_t *obj, ui_class_t cls)
{
    lv_obj_t *ind = lv_label_create(obj);
    lv_obj_add_flag(ind, LV_OBJ_FLAG_HIDDEN);

    if (cls == UI_CLS_BTN_SELECT) {
        lv_obj_align(ind, LV_ALIGN_LEFT_MID, S(6), 0);
        add_style_fa_small_label(ind);
        lv_label_set_text_static(ind, UI_ICON_PLAY);
    } else {
        lv_obj_set_size(ind, S(180), LV_SIZE_CONTENT);
        lv_obj_align(ind, LV_ALIGN_CENTER, 0, 0);
        add_style_label_large(ind);
        lv_obj_set_style_text_align(ind, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(ind, ">                   <");
    }

    lv_obj_add_event_cb(obj, on_focus_indicator, LV_EVENT_FOCUSED, ind);
    lv_obj_add_event_cb(obj, on_focus_indicator, LV_EVENT_DEFOCUSED, ind);
}

// ==================== 页面: 主菜单 mainmenu ====================
//   —— 宫格内容布局 / 宫格装饰块 / 入口图标表

// 主菜单宫格内容布局：图标居中、文字居中偏移。
static const ui_grid_layout_t lonetrail_grid_layout = {
    .icon_align = LV_ALIGN_CENTER, .icon_x = 16, .icon_y = 16, .icon_color = UI_C_PRIMARY,
    .text_align = LV_ALIGN_CENTER, .text_x = -8, .text_y = -35, .text_color = UI_C_COUNT,
};

// 主菜单宫格按钮的装饰块：右上 3 个强调色小方块 + 底部三色横条。
// 所有孤星宫格按钮都带这套装饰；颜色走主题角色 (PRIMARY / STRIPE1~3)。
static void build_content_lonetrail_grid(lv_obj_t *obj, ui_class_t cls)
{
    (void)cls;

    static const lv_coord_t      bar_x[3] = { -16, 16, 48 };
    static const ui_color_role_t bar_c[3] = { UI_C_STRIPE1, UI_C_STRIPE2, UI_C_STRIPE3 };
    static const lv_coord_t      dot_x[3] = { 66, 66, 59 };
    static const lv_coord_t      dot_y[3] = { 3, -4, -4 };

    // 底部三条横条 (青/金/暗红) —— TOP_LEFT (相对按钮左上角), 非中心定位
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_obj_create(obj);
        lv_obj_set_pos(b, S(bar_x[i]), S(95));
        lv_obj_set_size(b, S(32), S(8));
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_set_style_bg_color(b, ui_color(bar_c[i]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    }

    // 右上三个橙色小方块 —— TOP_LEFT (相对按钮左上角), 非中心定位
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_obj_create(obj);
        lv_obj_set_pos(b, S(dot_x[i]), S(dot_y[i]));
        lv_obj_set_size(b, S(5), S(5));
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_set_style_bg_color(b, ui_color(UI_C_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    }
}

// 主菜单 6 个入口的图标 (Lonetrail 版)
static const char *const lonetrail_menu_icons[UI_MENU_ICON_COUNT] = {
    [UI_MENU_ICON_OPLIST]   = UI_ICON_ID_BADGE,          // 干员   \uf2c1
    [UI_MENU_ICON_DISPIMG]  = UI_ICON_PERSON_CIRCLE_PLUS, // 扩列图 \ue541
    [UI_MENU_ICON_APPS]     = UI_ICON_CUBE,               // 应用   \uf1b2
    [UI_MENU_ICON_FILES]    = UI_ICON_FILE,               // 文件   \uf15b
    [UI_MENU_ICON_SETTINGS] = UI_ICON_GEAR,               // 设置   \uf013
    [UI_MENU_ICON_DEV]      = UI_ICON_COMPACT_DISC,       // 设备   \uf51f
};

// ==================== 样式类表 —— 按页分组 (顺序无关编译, 只为人读) ====================
const ui_class_def_t ui_theme_classes_lonetrail[] = {

    // ---- confirm 确认框 ----
    // UI_CLS_BTN_CONFIRM —— 确认框「确定」: 危险大红底, 聚焦变浅红; 图标 ✓
    [UI_CLS_BTN_CONFIRM] = {
        .name = "btn_confirm",
        .def = {
            .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_DANGER_FOCUS, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_DANGER_FOCUS, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_ICON, .label_font_px = 30,
        .label_text = UI_ICON_CIRCLE_CHECK,
        .build_content = build_content_lonetrail,
    },
    // UI_CLS_BTN_CANCEL —— 确认框「取消」: 中性灰底, 文字随主题; 图标 ✗
    [UI_CLS_BTN_CANCEL] = {
        .name = "btn_cancel",
        .def = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_MUTED, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_ACCENT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_ICON, .label_font_px = 30,
        .label_text = UI_ICON_CIRCLE_XMARK,
        .build_content = build_content_lonetrail,
    },

    // ---- usbselect USB功能选择 ----
    // UI_CLS_BTN_SELECT —— 功能项: 中性灰底, 聚焦变强调青; 中文 label (不覆盖字体)
    [UI_CLS_BTN_SELECT] = {
        .name = "btn_select",
        .def = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_ACCENT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_ACCENT, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_ACCENT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = build_content_lonetrail,
    },

    // ---- mainmenu 主菜单 ----
    // UI_CLS_MAIN_GRID —— 宫格按钮: 奶油底直角, 聚焦上移 5px + 阴影加大 (无焦点外框, 不改底色)
    [UI_CLS_MAIN_GRID] = {
        .name = "main_grid",
        .def = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .pad_top = 10, .pad_bottom = 10, .pad_left = 16, .pad_right = 16,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
            .translate_x = 0, .translate_y = 0,
        },
        .foc = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_TEXT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .pad_top = 10, .pad_bottom = 10, .pad_left = 16, .pad_right = 16,
            .shadow_width = 15, .shadow_ofs_x = 0, .shadow_ofs_y = 6, .shadow_opa = 200,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
            .translate_x = 0, .translate_y = -5,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .focus_anim = true,
        .grid_layout = &lonetrail_grid_layout,
        .build_content = build_content_lonetrail_grid,
    },
    // UI_CLS_ACTION_RESTART —— 底部「重启程序」: WARNING 金底
    [UI_CLS_ACTION_RESTART] = {
        .name = "action_restart",
        .def = {
            .bg = UI_C_WARNING, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_WARNING, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_ACTION_SHUTDOWN —— 底部「关机」: DANGER 暗红底, 聚焦变浅
    [UI_CLS_ACTION_SHUTDOWN] = {
        .name = "action_shutdown",
        .def = {
            .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },

    // ---- oplist 干员列表 ----
    // UI_CLS_OPLIST_ENTRY —— 干员条目: 同 Rhode 结构 (颜色走角色随主题翻转)。
    //   三态底色锁中性、左边框 10px 预占 (默认透明)，聚焦/排序只换边框色。
    [UI_CLS_OPLIST_ENTRY] = {
        .name = "oplist_entry",
        .def = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_ACCENT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_ACCENT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT,
            .border_opa = LV_OPA_COVER, .border_post = false,
        },
        .edit = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_EDIT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 10, .border_side = LV_BORDER_SIDE_LEFT,
            .border_opa = LV_OPA_COVER, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .focus_anim = true,
        .build_content = NULL,
    },
    // UI_CLS_OPLIST_FLAG_RES —— res 角标 (分辨率): 紧凑方角旗标
    [UI_CLS_OPLIST_FLAG_RES] = {
        .name = "oplist_flag_res",
        .def = {
            .bg = UI_C_FLAG_RES_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_FLAG_RES_TEXT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .pad_top = 3, .pad_bottom = 2, .pad_left = 8, .pad_right = 16,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 2, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_OPLIST_FLAG_SD —— sd 角标 (数据盘): 紧凑方角旗标, 白字
    [UI_CLS_OPLIST_FLAG_SD] = {
        .name = "oplist_flag_sd",
        .def = {
            .bg = UI_C_FLAG_SD_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .pad_top = 3, .pad_bottom = 2, .pad_left = 8, .pad_right = 16,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 2, .shadow_opa = LV_OPA_COVER,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_BTN_ACTION —— 底部动作按钮 (oplist 刷新/主菜单): 直角+阴影; 颜色走语义 fill
    [UI_CLS_BTN_ACTION] = {
        .name = "btn_action",
        .def = {
            .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
};

// ==================== 页面装饰 (全页通用) ====================
//   —— 橙色顶栏容器 (logo + 标题 + 三条风格条) + 底部三竖条

// 页头：橙色顶栏容器 (logo + 标题 + 三条风格条)。返回标题 label。
// 替代 common 的统一页头 ui_header，供各屏 (含主菜单) 在 Lonetrail 主题下使用。
static lv_obj_t *lonetrail_header(lv_obj_t *root, const char *title)
{
    // 顶栏容器 (9,0,186x38, 橙底, 直角, 阴影, pad 16)
    lv_obj_t *bar = lv_obj_create(root);
    lv_obj_set_pos(bar, S(9), S(0));
    lv_obj_set_size(bar, S(186), S(38));
    lv_obj_set_style_pad_top(bar, S(16), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(bar, S(16), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(bar, S(16), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(bar, S(16), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, ui_color(UI_C_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(bar, S(8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(bar, S(1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // 标题 (TOP_LEFT 相对内容区)
    lv_obj_t *t = lv_label_create(bar);
    lv_obj_set_pos(t, S(24), S(-12));
    add_style_label_large(t);
    lv_obj_set_style_text_color(t, ui_color(UI_C_HEADER_TITLE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(t, title);

    // logo (TOP_LEFT 相对内容区)
    lv_obj_t *logo = ui_header_logo(bar);
    lv_obj_set_pos(logo, S(-12), S(-14));

    // 容器内三条风格条 (青/金/暗红, 62x3)
    for (int i = 0; i < 3; i++) {
        lv_obj_t *s = lv_obj_create(bar);
        lv_obj_set_pos(s, S(-16 + i * 62), S(19));
        lv_obj_set_size(s, S(62), S(3));
        add_style_theme_stripe(s);
        lv_obj_set_style_bg_color(s, ui_color((ui_color_role_t)(UI_C_STRIPE1 + i)),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    }

    return t;
}

// 页面装饰：页头 + 底部左侧三竖条 (青/金/暗红)。
static void lonetrail_decorate(lv_obj_t *root, const char *title)
{
    lonetrail_header(root, title);

    // 底部左侧三竖条 (35x22 @ y=386/408/430, 青/金/暗红)
    for (int i = 0; i < 3; i++) {
        lv_obj_t *s = lv_obj_create(root);
        lv_obj_set_pos(s, S(0), S(386 + i * 22));
        lv_obj_set_size(s, S(35), S(22));
        lv_obj_set_style_pad_all(s, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s, ui_color((ui_color_role_t)(UI_C_STRIPE1 + i)),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(s, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(s, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    }
}

// ==================== 坐标偏移 ====================
//   —— 与 Rhode 同布局 (屏内共享一套新坐标)，故复用同套偏移。dx/dy/w/h 360 基准。
static const ui_ofs_t lonetrail_ofs[UI_OF_SLOT_COUNT] = {
    // ---- mainmenu ----
    [UI_OF_SLOT_MAIN_SLIDER]    = { .dy = 4,   .h = 5 },
    [UI_OF_SLOT_MAIN_VERSION]   = { .dx = 14 },
    [UI_OF_SLOT_MAIN_COPYRIGHT] = { .dx = 14 },
    // ---- oplist ----
    [UI_OF_SLOT_OPLIST_RES]     = { .dx = -19 },
    [UI_OF_SLOT_OPLIST_SD]      = { .dx = -19, .dy = -3 },
    // ---- confirm ----
    [UI_OF_SLOT_CONFIRM_CANCEL] = { .dx = -28, .dy = 5,  .w = 180, .h = 51 },
    [UI_OF_SLOT_CONFIRM_OK]     = { .dx = -7,  .dy = 6,  .w = 180, .h = 51 },
    // ---- usbselect ----
    [UI_OF_SLOT_USB_EPASS]      = { .dx = -19, .w = 160 },
    [UI_OF_SLOT_USB_FIDO]       = { .dx = 3,   .w = 160 },
    [UI_OF_SLOT_USB_MTP]        = { .dx = -19, .w = 160 },
    [UI_OF_SLOT_USB_CHARGE]     = { .dx = 3,   .w = 160 },
    // 未列出的槽位 (主菜单宫格/太阳/重启关机、confirm 图标/标题/正文、applist 角标等)
    // = 基准坐标 (0)，无需偏移。
};

// ==================== preset ====================
const ui_theme_preset_t ui_theme_preset_lonetrail = {
    .name = "Lonetrail",
    .dark = false,
    .advanced = true,
    .cls = ui_theme_classes_lonetrail,
    .menu_icons = lonetrail_menu_icons,
    .ofs = lonetrail_ofs,
    .decorate = lonetrail_decorate,
    .header = lonetrail_header,
    .gauge = NULL,   // 存储仪表用默认 arc
    // displayimg 计数器相对默认 (157,16) 左移 4、上移 12 (孤星顶栏内标题靠左)
    .size_label_dx = -4, .size_label_dy = -12,
    // 色板以浅色底 + 橙色强调为主基调；FOCUS/MUTED/SURFACE 等角色取近似值，
    //     以与整体观感协调。
    .pal = {
        // PRIMARY        橙          顶栏/图标/装饰块/主强调
        [UI_C_PRIMARY]=0xe17b41, [UI_C_PRIMARY_FOCUS]=0xe89a6b, // FOCUS 近似值
        // WARNING        金          重启按钮
        [UI_C_WARNING]=0xebab38,
        // DANGER         暗红        关机/格式化
        [UI_C_DANGER]=0x992926,  [UI_C_DANGER_FOCUS]=0xb04a45, // FOCUS 近似值
        // SUCCESS        青          确认操作
        [UI_C_SUCCESS]=0x5c7f7e,
        // ACCENT         青          聚焦外框/装饰
        [UI_C_ACCENT]=0x5c7f7e,
        // ACCENT2        金          次强调
        [UI_C_ACCENT2]=0xebab38,
        // EDIT           橙          排序模式拎起高亮条 (取 PRIMARY)
        [UI_C_EDIT]=0xe17b41,
        // NEUTRAL        深奶油      宫格按钮底
        [UI_C_NEUTRAL]=0xe0d6b8,
        // MUTED          灰褐        次要灰
        [UI_C_MUTED]=0x9c9484, // 近似值
        // SURFACE        浅奶油      面板底
        [UI_C_SURFACE]=0xf2ecd8, // 近似值
        // INFO           蓝          信息角标
        [UI_C_INFO]=0x5c7f7e,
        // ON_ACCENT      近白        强调底上的文字 (顶栏/重启/关机文字)
        [UI_C_ON_ACCENT]=0xf5eddc,
        // BG             奶油        页面底色
        [UI_C_BG]=0xf5eddb,
        // TEXT           深灰        正文/宫格按钮文字
        [UI_C_TEXT]=0x2f2e2e,
        // HEADER_TITLE   近白        屏标题 (主菜单 "主菜单", 橙底白字)
        [UI_C_HEADER_TITLE]=0xf5eddc,
        // STRIPE1~3      青/金/暗红   主题风格条三色
        [UI_C_STRIPE1]=0x8bc4b5,
        [UI_C_STRIPE2]=0xebab38,
        [UI_C_STRIPE3]=0x992926,
        // SLIDER         滑条       轨道/已填充(青)/旋钮
        [UI_C_SLIDER_TRACK]=0x313131, [UI_C_SLIDER_INDICATOR]=0x5c7f7e, [UI_C_SLIDER_KNOB]=0xececec,
        // GAUGE          存储仪表   轨道(深灰)/已填充(青)
        [UI_C_GAUGE_TRACK]=0x313131, [UI_C_GAUGE_INDICATOR]=0x5c7f7e,
        // FLAG_RES       角标       res 角标 (金底深字) / sd 角标 (橙底白字)
        [UI_C_FLAG_RES_BG]=0xebab38, [UI_C_FLAG_RES_TEXT]=0x2f2e2e,
        [UI_C_FLAG_SD_BG]=0xe17b41,
        // BTN_LIGHT      浅色按钮    oplist 刷新列表 (浅奶油底深字)
        [UI_C_BTN_LIGHT_BG]=0xfaf6ea, [UI_C_BTN_LIGHT_TEXT]=0x2f2e2e,
        // IMG_OUTLINE    头像描边    oplist logo 白边
        [UI_C_IMG_OUTLINE]=0xffffff
    },
};
