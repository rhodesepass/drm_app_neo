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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_C_PRIMARY = 0, UI_C_PRIMARY_FOCUS,
    UI_C_WARNING,
    UI_C_DANGER,  UI_C_DANGER_FOCUS,
    UI_C_SUCCESS,
    UI_C_ACCENT,          // 焦点青高亮 (列表条目聚焦态 / 焦点外框)
    UI_C_NEUTRAL,         // 列表条目底
    UI_C_MUTED,           // 取消/禁用/次要灰
    UI_C_SURFACE,         // 卡片/浅底 (文件管理器等)
    UI_C_INFO,            // SD 靛蓝角标
    UI_C_ON_ACCENT,       // 强调色底上的文字 (通常白)
    UI_C_BG,              // 屏幕背景
    UI_C_COUNT
} ui_color_role_t;

// 当前方案下该角色的颜色。
lv_color_t ui_color(ui_color_role_t r);

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
