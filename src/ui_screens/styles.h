#pragma once
//
// 手写 UI 的样式 (替代 EEZ 生成的 styles.c)
//
// 字体一律走 font_registry (运行时 FreeType)，不再引用编进二进制的烘焙字体。
// 样式为懒加载静态单例，首次使用时构建；调用前须先 font_registry_init()。
//
#include <lvgl/lvgl.h>

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
} ui_sem_t;

void add_style_label_large(lv_obj_t *obj);     // 标题 (FONT_TITLE)
void add_style_label_small(lv_obj_t *obj);     // 正文 (FONT_BODY)
void set_style_label_size(lv_obj_t *obj, bool large); // 二选一，切换标题/正文字号
void add_style_fa_label(lv_obj_t *obj);        // 图标 (FONT_ICON)
void add_style_main_btn(lv_obj_t *obj);        // 主菜单大按钮
void add_style_main_small_btn(lv_obj_t *obj);  // 主菜单小按钮 (重启/关机)

void add_style_op_btn(lv_obj_t *obj);          // 列表条目按钮 (干员/应用)
void add_style_op_entry(lv_obj_t *obj);        // 列表条目外层间距
void add_style_sd_flag(lv_obj_t *obj);         // "SD" 角标
void add_style_app_bg_running(lv_obj_t *obj);  // 应用"后台"角标
void add_style_app_fg(lv_obj_t *obj);          // 应用"前台"角标
void add_style_app_bg_notrunning(lv_obj_t *obj);// 应用"未运行"角标

void add_style_fill(lv_obj_t *obj, ui_sem_t sem); // 语义底色 (按钮/面板)
void add_style_spinner_arc(lv_obj_t *obj);        // spinner 弧 (中性)
void add_style_log_text(lv_obj_t *obj);           // 次要日志文字 (灰)
void add_style_focus(lv_obj_t *obj);              // 加粗焦点外框 (可聚焦控件)
void add_style_screen_bg(lv_obj_t *obj);          // 屏幕背景底 (随方案换)

// 按当前调色板 (重)着色所有共享 style；由 ui_theme_apply() 调用以随主题翻转。
void styles_apply_palette(void);

#ifdef __cplusplus
}
#endif
