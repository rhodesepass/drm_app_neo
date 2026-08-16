#pragma once
//
// ui_theme —— 运行时语义色板 + 命名配色方案 (UI 通用)
//
// 颜色不再散落成硬编码 hex，而是按语义角色查表。每套配色方案 (preset) 是一张色表 +
// 一个深/浅标记 (决定 LVGL 基础主题的卡片/文字/滚动条)。切方案时换表并重设 LVGL 主题
// + 重着色所有共享 style，一次 report_style_change 刷新全场。强调色 (primary/warning/
// danger/success) 随方案翻转；中性/背景/文字由方案 + LVGL 主题共同接管。
//
#include <lvgl/lvgl.h>
#include "font_registry.h"   // font_role_t (类描述符里用到子 label 字体角色)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_C_PRIMARY = 0, UI_C_PRIMARY_FOCUS,
    UI_C_WARNING,
    UI_C_DANGER,  UI_C_DANGER_FOCUS,
    UI_C_SUCCESS,
    UI_C_ACCENT,          // 装饰色 (oplist 聚焦外框 / 装饰元素)
    UI_C_ACCENT2,         // 次强调/装饰色 (干员条目描述左边框等)
    UI_C_EDIT,            // 编辑/排序态高亮 (oplist 排序拎起条)
    UI_C_NEUTRAL,         // 列表条目底
    UI_C_MUTED,           // 取消/禁用/次要灰
    UI_C_SURFACE,         // 卡片/浅底 (文件管理器等)
    UI_C_INFO,            // 角标（SD)
    UI_C_ON_ACCENT,       // 强调色底上的文字 (通常白)
    UI_C_BG,              // 屏幕背景
    UI_C_TEXT,            // 文字
    UI_C_HEADER_TITLE,    // 屏标题文字 (如主菜单 "主菜单")
    UI_C_STRIPE1,         // 主题风格条1
    UI_C_STRIPE2,         // 主题风格条2
    UI_C_STRIPE3,         // 主题风格条3
    UI_C_SLIDER_TRACK,    // 滑条轨道
    UI_C_SLIDER_INDICATOR,// 滑条已填充部分
    UI_C_SLIDER_KNOB,     // 滑条旋钮
    UI_C_GAUGE_TRACK,     // 存储仪表 (arc/bar) 轨道
    UI_C_GAUGE_INDICATOR, // 存储仪表 (arc/bar) 已填充
    UI_C_FLAG_RES_BG,     // res 角标底色 (各主题可不同)
    UI_C_FLAG_RES_TEXT,   // res 角标文字色
    UI_C_FLAG_SD_BG,      // sd 角标底色 (各主题可不同)
    UI_C_BTN_LIGHT_BG,    // 浅色按钮底 (oplist 刷新列表)
    UI_C_BTN_LIGHT_TEXT,  // 浅色按钮文字 (oplist 刷新列表)
    UI_C_IMG_OUTLINE,     // 图片/头像描边 (oplist logo 白边)
    UI_C_COUNT
} ui_color_role_t;

// ======================= 样式类 (CSS class 类似物) =======================
// 一个"类"把某类元素 (按钮/面板…) 的整套可主题化外观打包：底色/文字/圆角/阴影/
// 选中(聚焦)效果等。每套 preset 各带一张类表，切主题时整表应用到共享 style ——
// 于是半径、阴影、选中样式都能随主题变化，而不只是换颜色。
// 元素侧只需 add_class(obj, UI_CLS_BTN_CONFIRM)，具体长什么样全由主题决定。
typedef enum {
    UI_CLS_BTN_CONFIRM = 0,   // 确认按钮 (危险语义: 确认框"是")
    UI_CLS_BTN_CANCEL,        // 取消按钮 (中性语义: 确认框"否")
    UI_CLS_BTN_SELECT,        // 选择按钮 (usbselect 功能项)
    UI_CLS_MAIN_GRID,         // 主菜单宫格按钮 (图标+文字, 聚焦上移+阴影)
    UI_CLS_OPLIST_ENTRY,      // oplist 干员条目按钮 (聚焦/排序左侧高亮条)
    UI_CLS_OPLIST_FLAG_RES,   // oplist res 角标 (分辨率, 颜色/圆角随主题)
    UI_CLS_OPLIST_FLAG_SD,    // oplist sd 角标 (数据盘)
    UI_CLS_BTN_ACTION,        // 底部动作按钮形状 (直角/阴影, 颜色走语义 fill)
    UI_CLS_ACTION_RESTART,    // 主菜单 重启程序 (颜色/圆角随主题)
    UI_CLS_ACTION_SHUTDOWN,   // 主菜单 关机
    UI_CLS_COUNT
} ui_class_t;

// 一个状态 (默认 / 聚焦) 下的完整样式。coord 字段一律 360 基准，styles.c 应用时套 S()。
// 颜色角色填 UI_C_COUNT 表示"不设/继承" (如中性按钮文字随主题深浅自动适配)。
typedef struct {
    ui_color_role_t   bg;           // 底色角色
    ui_color_role_t   text;         // 文字色角色
    ui_color_role_t   border;       // 边框色角色
    ui_color_role_t   shadow;       // 阴影色角色 (UI_C_COUNT=默认黑)
    lv_opa_t          bg_opa;       // 底色不透明度
    lv_coord_t        radius;       // 圆角 (360 基准)
    lv_coord_t        pad_all;      // 内边距 (360 基准, 四方向统一)
    lv_coord_t        pad_top;      // 上内边距 (360 基准, 任一四方向字段非 0 时走四方向)
    lv_coord_t        pad_bottom;   // 下内边距 (360 基准)
    lv_coord_t        pad_left;     // 左内边距 (360 基准)
    lv_coord_t        pad_right;    // 右内边距 (360 基准)
    lv_coord_t        shadow_width; // 阴影宽 (360 基准)
    lv_coord_t        shadow_ofs_x; // 阴影 X 偏移 (360 基准)
    lv_coord_t        shadow_ofs_y; // 阴影 Y 偏移 (360 基准)
    lv_opa_t          shadow_opa;   // 阴影不透明度
    lv_coord_t        border_width; // 边框宽 (360 基准)
    lv_border_side_t  border_side;  // 边框侧
    lv_opa_t          border_opa;   // 边框不透明度
    bool              border_post;  // 边框画在子对象之上
    lv_coord_t        translate_x;  // 水平偏移 (360 基准, 聚焦上移等)
    lv_coord_t        translate_y;  // 垂直偏移 (360 基准, 聚焦上移等)
} ui_class_state_t;

// ======================= 主菜单宫格内容布局 =======================
// grid_btn 里 icon 与文字标签的对齐与偏移 (360 基准)。仅 UI_CLS_MAIN_GRID 使用。
// 对齐非 TOP_LEFT 时 x/y 是相对该对齐点的偏移；TOP_LEFT 时是绝对坐标。
typedef struct {
    lv_align_t        icon_align;  // 图标对齐 (TOP_LEFT=绝对坐标, CENTER=相对按钮中心)
    lv_coord_t        icon_x;      // 图标 x 偏移 (360 基准)
    lv_coord_t        icon_y;      // 图标 y 偏移 (360 基准)
    lv_coord_t        icon_w;      // 图标 label 固定宽度 (360 基准, 0=CONTENT; >0 时图标在框内居中)
    lv_coord_t        icon_h;      // 图标 label 固定高度 (360 基准, 0=CONTENT; 字形底部被裁切时加大)
    ui_color_role_t   icon_color;  // 图标文字色 (UI_C_COUNT=继承按钮文字色)
    lv_align_t        text_align;  // 文字对齐 (当前各主题均 LV_ALIGN_CENTER)
    lv_coord_t        text_x;      // 文字 x 偏移 (360 基准, 相对中心)
    lv_coord_t        text_y;      // 文字 y 偏移 (360 基准, 相对中心)
    ui_color_role_t   text_color;  // 文字色 (UI_C_COUNT=继承按钮文字色)
} ui_grid_layout_t;

// 一个类 = 名称 + 默认态 + 聚焦态 + 类级行为。
// 类级行为不属于某个状态：no_focus_ring 压制默认焦点外框；label_font_* 覆盖子 label 字体；
// w/h 覆盖按钮尺寸；build_content 添加/覆盖内容子节点。
typedef struct {
    const char       *name;            // 类名 (调试/日志用；查找键是 ui_class_t 枚举, 不是这个字符串)
    ui_class_state_t def;
    ui_class_state_t foc;
    ui_class_state_t edit;             // 编辑/排序态 (LV_STATE_USER_1, 如 oplist 排序拎起高亮)
    bool              no_focus_ring;    // true = add_class 时干掉默认焦点外框 (用 " >" 指示器等替代)
    font_role_t       label_font_role;  // 子 label 字体角色 (label_font_px>0 时生效)
    lv_coord_t        label_font_px;    // 子 label 字号 (360 基准, 0=不覆盖, 继承 ui_text_button 的默认)
    const char       *label_text;       // 覆盖首个子 label 文本 (如 confirm 的 "确定/取消" 或图标码点), NULL=不改
    lv_coord_t        w;                // 覆盖按钮宽 (360 基准, 0=不覆盖, 沿用 UI 文件 lv_obj_set_size 的宽)
    lv_coord_t        h;                // 覆盖按钮高 (360 基准, 0=不覆盖)
    bool              focus_anim;       // true = 聚焦/失焦带 150ms 过渡动画 (styles.c 统一挂)
    const ui_grid_layout_t *grid_layout; // 主菜单宫格内容布局 (仅 UI_CLS_MAIN_GRID, NULL=用默认)
    void (*build_content)(lv_obj_t *obj, ui_class_t cls);  // 内容/子节点钩子, NULL=不加额外子节点
} ui_class_def_t;

// ======================= 主菜单图标角色 =======================
// 主菜单 6 个宫格入口的图标随主题不同 (各 preset 的 menu_icons[] 一套)。
// preset 用 menu_icons[] 提供每个入口的 UTF-8 图标码点 (icons.h 的 UI_ICON_*)。
typedef enum {
    UI_MENU_ICON_OPLIST = 0,   // 干员
    UI_MENU_ICON_DISPIMG,      // 扩列图
    UI_MENU_ICON_APPS,         // 应用
    UI_MENU_ICON_FILES,        // 文件
    UI_MENU_ICON_SETTINGS,     // 设置
    UI_MENU_ICON_DEV,          // 设备
    UI_MENU_ICON_COUNT
} ui_menu_icon_t;

// ======================= 屏幕元素坐标偏移 =======================
// 屏内一律以「旧版 (main 分支)」坐标写基准；主题用 preset 的 ofs 表把元素平移到
// 自己的布局。每个可被主题平移的元素一个槽位 (按页面分组)；
// dx/dy/w/h 一律 360 基准，S() 由 ui_place 统一套。
// 按钮的尺寸/文字另走类表 (w/h、label_text)；纯坐标平移走这里 (可逐实例不同)。
typedef enum {
    // ---- mainmenu 主菜单 ----
    UI_OF_SLOT_MAIN_GRID_OPLIST,   // 宫格: 干员
    UI_OF_SLOT_MAIN_GRID_DISPIMG,  // 宫格: 扩列图
    UI_OF_SLOT_MAIN_GRID_APPS,     // 宫格: 应用
    UI_OF_SLOT_MAIN_GRID_FILES,    // 宫格: 文件
    UI_OF_SLOT_MAIN_GRID_SETTINGS, // 宫格: 设置
    UI_OF_SLOT_MAIN_GRID_DEV,      // 宫格: 设备
    UI_OF_SLOT_MAIN_SUN,           // 亮度太阳图标
    UI_OF_SLOT_MAIN_SLIDER,        // 亮度滑条
    UI_OF_SLOT_MAIN_VERSION,       // 版本号
    UI_OF_SLOT_MAIN_COPYRIGHT,     // 版权
    UI_OF_SLOT_MAIN_BTN_RESTART,   // 重启程序
    UI_OF_SLOT_MAIN_BTN_SHUTDOWN,  // 关机
    // ---- oplist 干员列表 ----
    UI_OF_SLOT_OPLIST_RES,         // 条目内 res 角标 (基准 281,26)
    UI_OF_SLOT_OPLIST_SD,          // 条目内 sd 角标 (基准 281,52)
    // ---- applist 应用列表 ----
    UI_OF_SLOT_APPLIST_STATE,      // 条目内 前/后台 角标 (基准 303,52)
    UI_OF_SLOT_APPLIST_SD,         // 条目内 sd 角标 (基准 313,26)
    // ---- confirm 确认框 ----
    UI_OF_SLOT_CONFIRM_ICON,       // 警示图标
    UI_OF_SLOT_CONFIRM_HEAD,       // 标题
    UI_OF_SLOT_CONFIRM_TITLE,      // 正文
    UI_OF_SLOT_CONFIRM_CANCEL,     // 取消按钮
    UI_OF_SLOT_CONFIRM_OK,         // 确定按钮
    // ---- usbselect USB选择 ----
    UI_OF_SLOT_USB_ICON,           // 页头图标
    UI_OF_SLOT_USB_HEAD,           // 页头标题
    UI_OF_SLOT_USB_HINT,           // 用途提示
    UI_OF_SLOT_USB_EPASS,          // 左上 (管理APP)
    UI_OF_SLOT_USB_FIDO,           // 右上 (FIDO密钥)
    UI_OF_SLOT_USB_MTP,            // 左下 (文件/MTP)
    UI_OF_SLOT_USB_CHARGE,         // 右下 (仅充电)
    // ---- sysinfo 设备信息 ----
    UI_OF_SLOT_SYS_LBL_NAND,       // "系统盘" 文本
    UI_OF_SLOT_SYS_LBL_SD,         // "数据盘" 文本
    UI_OF_SLOT_SYS_PCT_NAND,       // NAND 容量 label
    UI_OF_SLOT_SYS_PCT_SD,         // SD 容量 label
    UI_OF_SLOT_SYS_INFO,           // 设备信息文本
    UI_OF_SLOT_SYS_BTN_BACK,       // 返回
    UI_OF_SLOT_SYS_BTN_FORMAT,     // 格式化数据盘
    // ---- settings 设置 ----
    UI_OF_SLOT_SET_BTN_CACHE,      // 清除缓存
    UI_OF_SLOT_SET_SW_ROW1,        // 低电量自动关机 (label+switch 共享槽)
    UI_OF_SLOT_SET_SW_ROW2,        // 跳过入场动画
    UI_OF_SLOT_SET_SW_ROW3,        // 不显示信息层
    UI_OF_SLOT_SET_DD_MODE,        // 切换模式 (label+dropdown 共享槽)
    UI_OF_SLOT_SET_DD_INTERVAL,    // 自动切换间隔
    UI_OF_SLOT_SET_DD_THEME,       // 主题
    UI_OF_SLOT_SET_BTN_RESET,      // 重置USB模式
    UI_OF_SLOT_SET_BTN_BACK,       // 返回
    // ---- spinner 过场 ----
    UI_OF_SLOT_SPIN_SPINNER,       // 进度圈
    UI_OF_SLOT_SPIN_STATUS,        // 状态文字
    UI_OF_SLOT_SPIN_LOG,           // 日志文字
    // ---- warning 告警 ----
    UI_OF_SLOT_WARN_ICON,          // 告警图标
    UI_OF_SLOT_WARN_TITLE,         // 告警标题
    UI_OF_SLOT_WARN_DESC,          // 告警描述
    UI_OF_SLOT_COUNT
} ui_of_slot_t;

// 一个屏幕元素的坐标偏移 + 尺寸覆盖。
typedef struct {
    lv_coord_t dx, dy;   // 相对基准坐标偏移 (360 基准)
    lv_coord_t w, h;     // 尺寸覆盖 (0=不覆盖)
} ui_ofs_t;

// 存储仪表构建钩子：主题决定用 arc 还是 bar，并自行定位 (可叠加偏移)。
// root=屏根; slot=0 系统盘 1 数据盘; base_x/y/w/h=基准区域 (360 基准);
// pct_out 返回中心/旁侧百分比 label (供 tick 更新), 不可为 NULL。
typedef lv_obj_t *(*ui_theme_gauge_cb_t)(lv_obj_t *root, int slot,
        lv_coord_t base_x, lv_coord_t base_y, lv_coord_t base_w, lv_coord_t base_h,
        lv_obj_t **pct_out);

// ======================= 配色方案 (preset) =======================
// 一套主题 = 色表 + 深/浅标记 + 可选的高级样式类表。
// 具体 preset 定义在 src/ui/ui_theme/ 下的主题文件里 (如 theme_old.c)，
// 注册表 ui_theme_presets[] 在 src/ui/ui_theme/themes.c。ui_theme.c 只负责切换。
// decorate 是页面级装饰钩子：主题给页面根节点加内容 (顶栏/风格条/底部装饰条)。
typedef void (*ui_theme_decor_cb_t)(lv_obj_t *root, const char *title);
// header 是页头钩子：主题构建页头 (logo + 标题)，替代 common 的统一页头 ui_header。
typedef lv_obj_t *(*ui_theme_header_cb_t)(lv_obj_t *root, const char *title);

typedef struct {
    const char *name;              // 主题显示名 (设置屏下拉)
    bool        dark;              // LVGL 基础主题深浅
    bool        advanced;          // true = 类表来自独立主题文件 (theme_xxx.c)
    uint32_t    pal[UI_C_COUNT];   // 语义色表 (0xRRGGBB)
    const ui_class_def_t *cls;     // 高级样式类表 (advanced=true 时生效)
    const char *const *menu_icons; // 主菜单 6 个入口图标 (NULL=不提供, 查 UI_MENU_ICON_*)
    const ui_ofs_t *ofs;           // 屏幕元素坐标偏移表 ([UI_OF_SLOT_COUNT], NULL=全零)
    ui_theme_decor_cb_t decorate;  // 页面装饰钩子 (顶栏/风格条/底部装饰), NULL=回退 ui_header
    ui_theme_header_cb_t header;   // 页头钩子 (logo+标题), NULL=回退 ui_header
    ui_theme_gauge_cb_t gauge;     // 存储仪表构建钩子 (sysinfo), NULL=默认 arc
    const char *const *titles;     // 各屏标题覆盖表 (下标=screen_id_t, NULL 项=屏内默认标题)
    lv_coord_t size_label_dx;      // displayimg 图片大小 label x 偏移 (360 基准, 相对默认 157)
    lv_coord_t size_label_dy;      // displayimg 图片大小 label y 偏移 (360 基准, 相对默认 16)
} ui_theme_preset_t;

// 所有主题 preset 的注册表 (定义在 src/ui/ui_theme/themes.c)。
extern const ui_theme_preset_t *const ui_theme_presets[];
extern const int ui_theme_preset_count;

// 当前方案下该角色的颜色。
lv_color_t ui_color(ui_color_role_t r);

// 当前 preset 的类定义 (styles.c 用它给共享 style 整表重设)。
const ui_class_def_t *ui_theme_class(ui_class_t cls);

// 当前 preset 的主菜单入口图标 (UTF-8 码点字符串, 无则返回 "")。
const char *ui_theme_menu_icon(ui_menu_icon_t icon);

// 当前 preset 的页面装饰钩子：给页面根节点加内容 (顶栏/风格条/底部装饰条)。
// 主题未提供钩子时回退到 ui_header(root, title)。各屏在 create 时调用。
void ui_theme_decorate(lv_obj_t *root, const char *title);

// 当前 preset 的页头：构建页头 (logo + 标题)，返回标题 label。主题未提供 header 钩子时回退 ui_header。
lv_obj_t *ui_theme_header(lv_obj_t *root, const char *title);

// 当前 preset 下某屏的标题：覆盖表 (titles) 提供了则用之，否则返回传入的默认标题。
// 各屏以 ui_theme_header(root, ui_theme_title(SCREEN_X, "默认标题")) 调用，保留覆盖接口。
const char *ui_theme_title(int screen_id, const char *def);

// 当前 preset 下某屏幕元素的坐标偏移 + 尺寸覆盖 (无则返回全零)。
const ui_ofs_t *ui_theme_ofs(ui_of_slot_t slot);

// 当前 preset 的存储仪表：构建 arc/bar (sysinfo)。主题未提供 gauge 钩子时回退默认 arc。
lv_obj_t *ui_theme_gauge(lv_obj_t *root, int slot,
        lv_coord_t base_x, lv_coord_t base_y, lv_coord_t base_w, lv_coord_t base_h,
        lv_obj_t **pct_out);
// 内置默认 bar 仪表：preset 可 .gauge = ui_theme_gauge_bar 直接改用条形仪表。
// 与默认 arc (gauge=NULL) 共用 UI_C_GAUGE_* 主题色；tick 已按控件类型分派。
lv_obj_t *ui_theme_gauge_bar(lv_obj_t *root, int slot,
        lv_coord_t base_x, lv_coord_t base_y, lv_coord_t base_w, lv_coord_t base_h,
        lv_obj_t **pct_out);

// displayimg 图片大小 label 相对默认 (157,16) 的偏移 (360 基准, 主题可覆盖)。
void ui_theme_size_label_ofs(int *dx, int *dy);

// 配色方案数量 / 名称 (供设置屏下拉列举)。
int         ui_theme_count(void);
const char *ui_theme_name(int id);
int         ui_theme_current(void);
bool        ui_theme_is_dark(void);   // 当前方案是否深色底

// 切方案总入口：换色板 -> 重设 LVGL 默认主题(含中文字体) -> 重着色共享 style ->
// report_style_change。依赖 font_registry，须在 font_registry_init() 之后调用。
// id 越界会被夹到有效范围。
void ui_theme_apply(int id);

#ifdef __cplusplus
}
#endif
