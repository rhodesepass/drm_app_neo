#include "ui/ui_theme/theme_rhode.h"
#include "ui_screens/styles.h"
#include "ui_screens/screens/screen_common.h"
#include "icons.h"
#include "ui_metrics.h"

// Rhode 高级样式主题。coord 一律 360 基准，styles.c 应用时套 S()；
// 颜色角色 UI_C_COUNT = 继承主题默认。类级行为:
//   no_focus_ring  = 干掉默认焦点外框 (改由 " >" 指示器 / 宫格上移+阴影 等主题选中效果)
//   label_font_*   = 覆盖子 label 字体 (confirm 两个按钮是图标字形)
//   build_content  = 给按钮添加选中指示器 (内容子节点)
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
static void build_content_rhode(lv_obj_t *obj, ui_class_t cls)
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
//   —— 宫格内容布局 / 入口图标表

// 主菜单宫格内容布局：图标左上(TOP_LEFT)、文字居中偏移。
static const ui_grid_layout_t rhode_grid_layout = {
    .icon_align = LV_ALIGN_TOP_LEFT, .icon_x = 6, .icon_y = 22, .icon_w = 55, .icon_h = 60, .icon_color = UI_C_MUTED,
    .text_align = LV_ALIGN_CENTER, .text_x = 1, .text_y = -30, .text_color = UI_C_COUNT,
};

// 主菜单宫格按钮的选中指示器：按钮底部小条条。
// 聚焦淡入 / 失焦淡出 (150ms, 与按钮聚焦动画同节奏)；颜色走 PRIMARY。
// 用「状态 + 过渡」而非 HIDDEN 即时切换：DEFAULT bg_opa=0 (透明), FOCUSED bg_opa=COVER,
// DEFAULT 样式挂过渡 → 条条与按钮上移/阴影动画同步, 不突兀。
static void on_grid_bar_focus(lv_event_t *e)
{
    lv_obj_t *bar = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_add_state(bar, LV_STATE_FOCUSED);
    else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED)
        lv_obj_remove_state(bar, LV_STATE_FOCUSED);
}

static void build_content_rhode_grid(lv_obj_t *obj, ui_class_t cls)
{
    (void)cls;

    // 条条过渡: 只动画 bg_opa, 150ms 缓出 (与按钮类聚焦动画同节奏)
    static lv_style_prop_t bar_props[] = {
        LV_STYLE_BG_OPA,
        LV_STYLE_PROP_INV,
    };
    static const lv_style_transition_dsc_t bar_trans = {
        .props    = bar_props,
        .time     = 150,
        .delay    = 0,
        .path_xcb = lv_anim_path_ease_out,
    };

    lv_obj_t *bar = lv_obj_create(obj);
    lv_obj_set_pos(bar, S(11), S(90));   // 相对按钮左上角
    lv_obj_set_size(bar, S(40), S(4));
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, ui_color(UI_C_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transition(bar, &bar_trans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(obj, on_grid_bar_focus, LV_EVENT_FOCUSED, bar);
    lv_obj_add_event_cb(obj, on_grid_bar_focus, LV_EVENT_DEFOCUSED, bar);
}

// 主菜单 6 个入口的图标 (Rhode 版)
static const char *const rhode_menu_icons[UI_MENU_ICON_COUNT] = {
    [UI_MENU_ICON_OPLIST]   = UI_ICON_ID_BADGE,           // 干员   \uf2c1
    [UI_MENU_ICON_DISPIMG]  = UI_ICON_PERSON_CIRCLE_PLUS, // 扩列图 \ue541
    [UI_MENU_ICON_APPS]     = UI_ICON_CUBE,               // 应用   \uf1b2
    [UI_MENU_ICON_FILES]    = UI_ICON_COPY,               // 文件   \uf0c5
    [UI_MENU_ICON_SETTINGS] = UI_ICON_GEAR,               // 设置   \uf013
    [UI_MENU_ICON_DEV]      = UI_ICON_DATABASE,           // 设备   \uf1c0
};

// ==================== 样式类表 —— 按页分组 (顺序无关编译, 只为人读) ====================
const ui_class_def_t ui_theme_classes_rhode[] = {

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
        .build_content = build_content_rhode,
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
        .build_content = build_content_rhode,
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
        .build_content = build_content_rhode,
    },

    // ---- mainmenu 主菜单 ----
    // UI_CLS_MAIN_GRID —— 宫格按钮: 卡片灰底, 聚焦变蓝 + 上移 5px + 阴影加大 (无焦点外框)
    [UI_CLS_MAIN_GRID] = {
        .name = "main_grid",
        .def = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 0, .pad_all = 0,
            .pad_top = 10, .pad_bottom = 10, .pad_left = 16, .pad_right = 16,
            .shadow_width = 8, .shadow_ofs_x = 0, .shadow_ofs_y = 1, .shadow_opa = LV_OPA_50,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
            .translate_x = 0, .translate_y = 0,
        },
        .foc = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
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
        .grid_layout = &rhode_grid_layout,
        .build_content = build_content_rhode_grid,
    },
    // UI_CLS_ACTION_RESTART —— 底部「重启程序」: WARNING 黄底
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
    // UI_CLS_ACTION_SHUTDOWN —— 底部「关机」: DANGER 红底, 聚焦变浅红
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
    // UI_CLS_OPLIST_ENTRY —— 干员条目: 中性底直角, 聚焦左侧 ACCENT 高亮条, 排序拎起 EDIT 橙。
    //   三态底色锁中性、左边框 10px 预占 (默认透明)，聚焦/排序只换边框色不挤布局。
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
//   —— 页头 + 标题下主题风格条 + 底部蓝色装饰条

static void rhode_decorate(lv_obj_t *root, const char *title)
{
    lv_obj_t *t = ui_header(root, title);
    lv_obj_set_style_text_color(t, ui_color(UI_C_HEADER_TITLE), LV_PART_MAIN | LV_STATE_DEFAULT);

    // 标题下三条主题风格条 (62x3 @ y=35, 颜色随 STRIPE1~3)
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bar = lv_obj_create(root);
        lv_obj_set_pos(bar, S(57 + i * 62), S(35));
        lv_obj_set_size(bar, S(62), S(3));
        add_style_theme_stripe(bar);
        lv_obj_set_style_bg_color(bar, ui_color((ui_color_role_t)(UI_C_STRIPE1 + i)),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    }

    // 底部左侧装饰条 (6x61 @ 25,389)
    lv_obj_t *bar = lv_obj_create(root);
    lv_obj_set_pos(bar, S(25), S(385));
    lv_obj_set_size(bar, S(6), S(61));
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, ui_color(UI_C_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
}

// ==================== 坐标偏移 ====================
//   —— 屏内基准为旧版 (main) 坐标, 这里平移到 Rhode 布局。dx/dy/w/h 360 基准; w/h=0 不覆盖。
static const ui_ofs_t rhode_ofs[UI_OF_SLOT_COUNT] = {
    // ---- mainmenu ----
    [UI_OF_SLOT_MAIN_SLIDER]    = { .dy = 4,   .h = 5 },    // 旧(60,300)高10 → (60,304)高5
    [UI_OF_SLOT_MAIN_VERSION]   = { .dx = 14 },
    [UI_OF_SLOT_MAIN_COPYRIGHT] = { .dx = 14 },
    // ---- oplist ----
    [UI_OF_SLOT_OPLIST_RES]     = { .dx = -19 },             // 旧(281,26) → (262,26)
    [UI_OF_SLOT_OPLIST_SD]      = { .dx = -19, .dy = -3 },   // 旧(281,52) → (262,49)
    // ---- confirm ----
    [UI_OF_SLOT_CONFIRM_CANCEL] = { .dx = -28, .dy = 5,  .w = 180, .h = 51 }, // 旧(28,70)149x51 → 满宽(0,75)180x51
    [UI_OF_SLOT_CONFIRM_OK]     = { .dx = -7,  .dy = 6,  .w = 180, .h = 51 }, // 旧(187,69)147x52 → (180,75)180x51
    // ---- usbselect ----
    [UI_OF_SLOT_USB_EPASS]      = { .dx = -19, .w = 160 }, // 左列
    [UI_OF_SLOT_USB_FIDO]       = { .dx = 3,   .w = 160 }, // 右列
    [UI_OF_SLOT_USB_MTP]        = { .dx = -19, .w = 160 },
    [UI_OF_SLOT_USB_CHARGE]     = { .dx = 3,   .w = 160 },
    // 未列出的槽位 (主菜单宫格/太阳/重启关机、confirm 图标/标题/正文、applist 角标等)
    // = 基准坐标 (0)，无需偏移。
};

// ==================== preset ====================
const ui_theme_preset_t ui_theme_preset_rhode = {
    .name = "Rhode",
    .dark = true,
    .advanced = true,
    .cls = ui_theme_classes_rhode,
    .menu_icons = rhode_menu_icons,
    .ofs = rhode_ofs,
    .decorate = rhode_decorate,
    .gauge = NULL,   // 存储仪表用默认 arc
    .pal = {
        // PRIMARY        高亮蓝     聚焦/SD角标/主菜单按钮
        [UI_C_PRIMARY]=0x4aabea, [UI_C_PRIMARY_FOCUS]=0x6cc3f5,
        // WARNING        警示黄     res角标
        [UI_C_WARNING]=0xf1c644,
        // DANGER         危险红     关机/格式化
        [UI_C_DANGER]=0xd23b3b,  [UI_C_DANGER_FOCUS]=0xe05555,
        // SUCCESS        成功青     确认操作
        [UI_C_SUCCESS]=0x5ABFB9,
        // ACCENT         装饰青     聚焦外框/spinner
        [UI_C_ACCENT]=0x5ABFB9,
        // ACCENT2        次强调黄   干员条目描述左边框
        [UI_C_ACCENT2]=0xe3cd56,
        // EDIT           排序橙     排序模式拎起高亮条
        [UI_C_EDIT]=0xF07000,
        // NEUTRAL        卡片灰     oplist条目底
        [UI_C_NEUTRAL]=0x444444,
        // MUTED          图标灰     icon色
        [UI_C_MUTED]=0x808080,
        // SURFACE        面板灰     大背景/中性按钮底
        [UI_C_SURFACE]=0x3a3a3a,
        // INFO           信息蓝     SD 角标
        [UI_C_INFO]=0x4aabea,
        // ON_ACCENT      强调白     高亮底上的文字/主题色条文字
        [UI_C_ON_ACCENT]=0xffffff,
        // BG             屏幕灰     页面底色
        [UI_C_BG]=0x515151,
        // TEXT           正文白     正文/版权/版本文字 (深色主题下白)
        [UI_C_TEXT]=0xffffff,
        // HEADER_TITLE   标题白     屏标题 (主菜单 "主菜单")
        [UI_C_HEADER_TITLE]=0xffffff,
        // STRIPE1~3      装饰白     主题风格条(当前同色)
        [UI_C_STRIPE1]=0xffffff,
        [UI_C_STRIPE2]=0xffffff,
        [UI_C_STRIPE3]=0xffffff,
        // SLIDER         滑条       轨道/已填充/旋钮
        [UI_C_SLIDER_TRACK]=0x313131, [UI_C_SLIDER_INDICATOR]=0x4aabea, [UI_C_SLIDER_KNOB]=0xececec,
        // GAUGE          存储仪表   轨道(深灰)/已填充(亮蓝)
        [UI_C_GAUGE_TRACK]=0x313131, [UI_C_GAUGE_INDICATOR]=0x4aabea,
        // FLAG_RES       角标       res 角标 (黄底深字) / sd 角标 (蓝底白字)
        [UI_C_FLAG_RES_BG]=0xf1c644, [UI_C_FLAG_RES_TEXT]=0x525151,
        [UI_C_FLAG_SD_BG]=0x4aabea,
        // BTN_LIGHT      浅色按钮    oplist 刷新列表 (白底黑字)
        [UI_C_BTN_LIGHT_BG]=0xffffff, [UI_C_BTN_LIGHT_TEXT]=0x000000,
        // IMG_OUTLINE    头像描边    oplist logo 白边
        [UI_C_IMG_OUTLINE]=0xffffff
    },
};
