#pragma once
//
// 各屏共用的小工具：根容器、统一页头(logo+标题)、文字按钮、自动聚焦。
// 不是 EEZ 那种全局表，只是消除 9 个屏的重复样板。
//
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// 图片目录(含 lv_fs 盘符)。sim 用 -DUI_IMG_DIR 注入仓库 font/；设备侧不定义此宏，
// logo 路径运行时按可执行文件同级 res/ 解析 (见 screen_common.c)。PRTS logo 按 720(2x) 出图。
#ifdef UI_IMG_DIR
#define LOGO_PRTS_PATH UI_IMG_DIR "/prts_64_inv.png"
#endif
#define LOGO_PRTS_FILE "prts_64_inv.png"

// 创建一个满屏根容器 (S 缩放后的 360x640)，无内边距。
lv_obj_t *ui_screen_root(void);
// 同上但不挂自动聚焦 cb；需要自管导航 group 的屏(虚拟滚动列表)用。
lv_obj_t *ui_screen_root_bare(void);

// 左上角统一页头：PRTS logo + 标题。
void ui_header(lv_obj_t *root, const char *title);

// 居中文字大按钮；bg=0 用主题默认色；cb=NULL 不挂回调。
lv_obj_t *ui_text_button(lv_obj_t *root, int x, int y, int w, int h,
                         uint32_t bg, const char *text, lv_event_cb_t cb);
lv_obj_t *ui_small_text_button(lv_obj_t *root, int x, int y, int w, int h,
                         uint32_t bg, const char *text, lv_event_cb_t cb);

// 进屏 LOAD_START 时自动把所有可聚焦后代(按钮/下拉/开关/滑条/roller)注册到导航 group。
// 各屏 root 挂上它即可，无需手写 focus 列表。
void ui_autofocus_cb(lv_event_t *e);

#ifdef __cplusplus
}
#endif
