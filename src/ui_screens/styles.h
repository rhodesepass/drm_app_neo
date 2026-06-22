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

void add_style_label_large(lv_obj_t *obj);     // 标题 (FONT_TITLE)
void add_style_label_small(lv_obj_t *obj);     // 正文 (FONT_BODY)
void add_style_fa_label(lv_obj_t *obj);        // 图标 (FONT_ICON)
void add_style_main_btn(lv_obj_t *obj);        // 主菜单大按钮
void add_style_main_small_btn(lv_obj_t *obj);  // 主菜单小按钮 (重启/关机)

#ifdef __cplusplus
}
#endif
