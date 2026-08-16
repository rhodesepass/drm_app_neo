#pragma once
//
// 手写 UI 的样式 (替代 EEZ 生成的 styles.c)
//
// 字体一律走 font_registry (运行时 FreeType)，不再引用编进二进制的烘焙字体。
// 样式为懒加载静态单例，首次使用时构建；调用前须先 font_registry_init()。
//
#include <lvgl/lvgl.h>
#include "ui/ui_theme.h"   // ui_class_t / ui_class_def_t / ui_theme_class

#ifdef __cplusplus
extern "C" {
#endif

// 语义底色 (随主题翻转)，用于按钮/面板底。DEFAULT = 走 LVGL 主题默认底。
typedef enum {
    UI_SEM_DEFAULT = 0,
    UI_SEM_PRIMARY,
    UI_SEM_WARNING,
    UI_SEM_DANGER,
    UI_SEM_SUCCESS,
    UI_SEM_NEUTRAL,
    UI_SEM_LIGHT,     // 浅色底按钮 (oplist 刷新列表: 浅底深字)
} ui_sem_t;

void add_style_label_large(lv_obj_t *obj);     // 标题 (FONT_TITLE)
void add_style_label_small(lv_obj_t *obj);     // 正文 (FONT_BODY)
void set_style_label_size(lv_obj_t *obj, bool large); // 二选一，切换标题/正文字号
void add_style_fa_label(lv_obj_t *obj);        // 图标 (FONT_ICON)
void add_style_fa_small_label(lv_obj_t *obj); // 小图标 (FONT_ICON @ 24)

void add_style_op_btn(lv_obj_t *obj);          // 列表条目按钮 (干员/应用)
void add_style_op_entry(lv_obj_t *obj);        // 列表条目外层间距
void add_style_sd_flag(lv_obj_t *obj);         // 数据盘来源角标 (资源位于 /sd 数据盘)
void add_style_res_flag(lv_obj_t *obj);        // 分辨率角标 "360"/"480"/"720"
void add_style_flag_compact(lv_obj_t *obj);    // 紧凑角标 (直角/对称pad/阴影, oplist 用)
void add_style_app_bg_running(lv_obj_t *obj);  // 应用"后台"角标
void add_style_app_fg(lv_obj_t *obj);          // 应用"前台"角标
void add_style_app_bg_notrunning(lv_obj_t *obj);// 应用"未运行"角标

void add_style_fill(lv_obj_t *obj, ui_sem_t sem); // 语义底色 (按钮/面板)
void add_style_spinner_arc(lv_obj_t *obj);        // spinner 弧 (中性)
void add_style_log_text(lv_obj_t *obj);           // 次要日志文字 (灰)
void add_style_main_slider(lv_obj_t *obj);        // 亮度滑条 (轨道/填充/旋钮, 随主题)
void add_style_gauge(lv_obj_t *obj);              // 存储仪表 (arc/bar 轨道/已填充, 随主题)
void add_style_switch(lv_obj_t *obj);             // 开关 (轨道/填充/旋钮, 随主题)
void add_style_dropdown(lv_obj_t *obj);           // 下拉 (主框/展开列表/选中项, 随主题)
void add_style_focus(lv_obj_t *obj);              // 加粗焦点外框 (可聚焦控件)
void add_style_screen_bg(lv_obj_t *obj);          // 屏幕背景底 (随方案换)
void add_style_theme_stripe(lv_obj_t *obj);       // 主题风格条 (页头/装饰条, 随主题翻转颜色)

// 挂一个样式类 (CSS class 类似物)：默认态 + 聚焦(选中)态，完整外观由当前主题的类表决定。
void add_class(lv_obj_t *obj, ui_class_t cls);

// 按当前调色板 (重)着色所有共享 style；由 ui_theme_apply() 调用以随主题翻转。
void styles_apply_palette(void);

#ifdef __cplusplus
}
#endif
