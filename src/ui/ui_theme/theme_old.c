#include "ui/ui_theme/theme_old.h"
#include "icons.h"

// Old —— 复刻修改前 (main 分支) 的默认「深色」主题。
// 类表颜色字段填角色 (UI_C_*)，切到本主题时由下方 palette 提供实际 hex。
// 设计取向：全部走「整块换色」而非边框/装饰块/指示器，
// 不压制默认焦点外框、不带聚焦过渡动画，贴近旧共享样式 (add_style_*) 的观感；
// 坐标/文字走屏内基准 (旧版坐标)，ofs=NULL 无需偏移。
// 本文件按「页面」分组，类表条目顺序与人读无关 (designated initializer)。

// ==================== 页面: 主菜单 mainmenu ====================
//   —— 宫格内容布局 / 入口图标表

// 旧主菜单宫格内容布局：图标顶部居中 (下移10)、文字底部居中 (上移8)。
static const ui_grid_layout_t old_grid_layout = {
    .icon_align = LV_ALIGN_TOP_MID, .icon_x = 0, .icon_y = 10,
    .icon_w = 0, .icon_h = 0, .icon_color = UI_C_COUNT,
    .text_align = LV_ALIGN_BOTTOM_MID, .text_x = 0, .text_y = -8, .text_color = UI_C_COUNT,
};

// 主菜单 6 个入口图标 (main 分支 grid_btn 用过的旧码点)
static const char *const old_menu_icons[UI_MENU_ICON_COUNT] = {
    [UI_MENU_ICON_OPLIST]   = UI_ICON_USER,          // 干员   \uf007
    [UI_MENU_ICON_DISPIMG]  = UI_ICON_IMAGES,        // 扩列图 \uf302
    [UI_MENU_ICON_APPS]     = UI_ICON_BOX_ARCHIVE,   // 应用   \uf187
    [UI_MENU_ICON_FILES]    = UI_ICON_FILE,          // 文件   \uf15b
    [UI_MENU_ICON_SETTINGS] = UI_ICON_GEAR,          // 设置   \uf013
    [UI_MENU_ICON_DEV]      = UI_ICON_MOBILE_SCREEN, // 设备   \uf3cf
};

// ==================== 样式类表 —— 按页分组 (顺序无关编译, 只为人读) ====================
// 所有按钮 radius=-1 → 不设圆角，走 LVGL 默认主题圆角 (旧版默认圆角按钮)。
const ui_class_def_t ui_theme_classes_old[] = {

    // ---- confirm 确认框 ----
    // UI_CLS_BTN_CONFIRM —— 确认框「确定」: 危险红底, 聚焦变浅红 (旧 UI_SEM_DANGER fill)。
    //   文字/坐标走屏内基准 (旧版): "确定" (187,69) 147x52
    [UI_CLS_BTN_CONFIRM] = {
        .name = "btn_confirm",
        .def = {
            .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_BTN_CANCEL —— 确认框「取消」: 中性灰底, 聚焦不变 (旧 UI_SEM_NEUTRAL fill)。
    //   文字/坐标走屏内基准 (旧版): "取消" (28,70) 149x51
    [UI_CLS_BTN_CANCEL] = {
        .name = "btn_cancel",
        .def = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },

    // ---- usbselect USB功能选择 ----
    // UI_CLS_BTN_SELECT —— 功能项: 中性灰底, 聚焦不变 (旧 UI_SEM_NEUTRAL fill)；文字由屏提供
    [UI_CLS_BTN_SELECT] = {
        .name = "btn_select",
        .def = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_MUTED, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },

    // ---- mainmenu 主菜单 ----
    // UI_CLS_MAIN_GRID —— 宫格按钮: PRIMARY 底, 聚焦变 PRIMARY_FOCUS (旧主菜单宫格按钮样式)
    [UI_CLS_MAIN_GRID] = {
        .name = "main_grid",
        .def = {
            .bg = UI_C_PRIMARY, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
            .translate_x = 0, .translate_y = 0,
        },
        .foc = {
            .bg = UI_C_PRIMARY_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
            .translate_x = 0, .translate_y = 0,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .focus_anim = false,
        .grid_layout = &old_grid_layout,
        .build_content = NULL,
    },
    // UI_CLS_ACTION_RESTART —— 底部「重启程序」: 红底 (旧底部小按钮用 DANGER)
    [UI_CLS_ACTION_RESTART] = {
        .name = "action_restart",
        .def = {
            .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_ACTION_SHUTDOWN —— 底部「关机」: 红底 (与重启同为 DANGER, 还原变更前)
    [UI_CLS_ACTION_SHUTDOWN] = {
        .name = "action_shutdown",
        .def = {
            .bg = UI_C_DANGER, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_DANGER_FOCUS, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_TITLE, .label_font_px = 0,
        .build_content = NULL,
    },

    // ---- oplist 干员列表 ----
    // UI_CLS_OPLIST_ENTRY —— 干员条目: 整块换色聚焦 灰底→青底, 排序整块橙 (旧 add_style_op_btn + USER_1)
    [UI_CLS_OPLIST_ENTRY] = {
        .name = "oplist_entry",
        .def = {
            .bg = UI_C_NEUTRAL, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_ACCENT, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .edit = {
            .bg = UI_C_EDIT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 8,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .focus_anim = false,
        .build_content = NULL,
    },
    // UI_CLS_OPLIST_FLAG_RES —— res 角标 (分辨率): 旧式圆角旗标 (圆角15, 上下 5/1, 左右 2)
    [UI_CLS_OPLIST_FLAG_RES] = {
        .name = "oplist_flag_res",
        .def = {
            .bg = UI_C_FLAG_RES_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_FLAG_RES_TEXT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 15, .pad_all = 0,
            .pad_top = 5, .pad_bottom = 1, .pad_left = 2, .pad_right = 2,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_OPLIST_FLAG_SD —— sd 角标 (数据盘): 旧式圆角旗标
    [UI_CLS_OPLIST_FLAG_SD] = {
        .name = "oplist_flag_sd",
        .def = {
            .bg = UI_C_FLAG_SD_BG, .bg_opa = LV_OPA_COVER, .text = UI_C_ON_ACCENT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = 15, .pad_all = 0,
            .pad_top = 5, .pad_bottom = 1, .pad_left = 2, .pad_right = 2,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = true,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
    // UI_CLS_BTN_ACTION —— 底部动作按钮 (oplist 刷新/主菜单): 只定形状 (默认圆角、无阴影); 颜色走语义 fill
    [UI_CLS_BTN_ACTION] = {
        .name = "btn_action",
        .def = {
            .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .foc = {
            .bg = UI_C_COUNT, .bg_opa = LV_OPA_COVER, .text = UI_C_COUNT,
            .border = UI_C_COUNT, .shadow = UI_C_COUNT,
            .radius = -1, .pad_all = 0,
            .shadow_width = 0, .shadow_ofs_x = 0, .shadow_ofs_y = 0, .shadow_opa = LV_OPA_TRANSP,
            .border_width = 0, .border_side = LV_BORDER_SIDE_NONE,
            .border_opa = LV_OPA_TRANSP, .border_post = false,
        },
        .no_focus_ring = false,
        .label_font_role = FONT_BODY, .label_font_px = 0,
        .build_content = NULL,
    },
};

// ==================== 页面装饰 (全页通用) ====================
//   —— 无自定义钩子: decorate/header 留空 → 回退 common 的 ui_header (旧版简单页头, 无风格条/装饰)

// ==================== 坐标偏移 ====================
//   —— 屏内基准即旧版布局, ofs=NULL 无需偏移。

// ==================== preset ====================
const ui_theme_preset_t ui_theme_preset_old = {
    .name = "Old 深色",
    .dark = true,
    .advanced = true,
    .cls = ui_theme_classes_old,
    .menu_icons = old_menu_icons,
    .ofs = NULL,        // 屏内基准即旧版布局, 无需偏移
    .decorate = NULL,   // 回退 common ui_header (旧版简单页头)
    .header = NULL,
    .gauge = NULL,      // 存储仪表用默认 arc
    .size_label_dx = 0, .size_label_dy = 0,
    // 色表取自旧版 (main 分支) 的「深色」预设；新增角色取近似值以匹配整体观感。
    .pal = {
        // PRIMARY        深蓝        主菜单宫格/按钮
        [UI_C_PRIMARY]=0x20679f, [UI_C_PRIMARY_FOCUS]=0x398ed0,
        // WARNING        暗金        警示
        [UI_C_WARNING]=0x8b7200,
        // DANGER         红          确认/关机
        [UI_C_DANGER]=0xb93030,  [UI_C_DANGER_FOCUS]=0xa63737,
        // SUCCESS        绿          成功
        [UI_C_SUCCESS]=0x149b5b,
        // ACCENT         青          列表条目聚焦底/焦点外框
        [UI_C_ACCENT]=0x67d9ec,
        // ACCENT2        青          (旧无此角色, 沿用 accent)
        [UI_C_ACCENT2]=0x67d9ec,
        // EDIT           橙          排序模式整块高亮
        [UI_C_EDIT]=0xF07000,
        // NEUTRAL        灰          列表条目底
        [UI_C_NEUTRAL]=0x494947,
        // MUTED          次要灰      取消/禁用
        [UI_C_MUTED]=0x919197,
        // SURFACE        面板灰
        [UI_C_SURFACE]=0x3a3a3a,
        // INFO           靛蓝        SD 角标
        [UI_C_INFO]=0x2c3cbd,
        // ON_ACCENT      白          强调底上的文字
        [UI_C_ON_ACCENT]=0xffffff,
        // BG             近黑        页面底色
        [UI_C_BG]=0x1a1a1a,
        // TEXT           白          正文
        [UI_C_TEXT]=0xffffff,
        // HEADER_TITLE   白          屏标题
        [UI_C_HEADER_TITLE]=0xffffff,
        // STRIPE1~3      白          主题风格条 (旧无此角色)
        [UI_C_STRIPE1]=0xffffff, [UI_C_STRIPE2]=0xffffff, [UI_C_STRIPE3]=0xffffff,
        // SLIDER         滑条
        [UI_C_SLIDER_TRACK]=0x313131, [UI_C_SLIDER_INDICATOR]=0x20679f, [UI_C_SLIDER_KNOB]=0xececec,
        // GAUGE          存储仪表   轨道(深灰)/已填充(深蓝)
        [UI_C_GAUGE_TRACK]=0x313131, [UI_C_GAUGE_INDICATOR]=0x20679f,
        // FLAG_RES       角标       res 角标 (深蓝底白字) / sd 角标 (靛蓝底白字)
        [UI_C_FLAG_RES_BG]=0x20679f, [UI_C_FLAG_RES_TEXT]=0xffffff,
        [UI_C_FLAG_SD_BG]=0x2c3cbd,
        // BTN_LIGHT      浅色按钮    刷新列表 (白底黑字)
        [UI_C_BTN_LIGHT_BG]=0xffffff, [UI_C_BTN_LIGHT_TEXT]=0x000000,
        // IMG_OUTLINE    白          头像描边
        [UI_C_IMG_OUTLINE]=0xffffff
    },
};
