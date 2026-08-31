#pragma once
//
// ui_preview —— 调试用 UI 预览 (仅 PC/sim 侧有意义)。
//
// 命令行传 `preview [选项]` 进入预览:
//   --theme <名|id>    指定主题 (如 old, 大小写不敏感; 也接受数字下标;
//                      缺省用存档主题)。只在启动时应用一次,不写回设置。
//   --screen <页名>    只显示指定页面,不轮播;缺省轮播全部弹窗。
//                      页面名: warning / confirm(大字) / uix,fido(小字) / usbselect /
//                      mainmenu / oplist / sysinfo / settings / applist。
//   [间隔ms]           轮播间隔毫秒 (缺省 3000;单屏预览时忽略)。
//
// main 解析命令行后写入 g_ui_preview_* 全局;lvgl_drm_warp 按 g_ui_preview_theme_id
// 决定启动主题;LVGL 初始化完成后 ui_preview_start() 建一个 lv_timer 依次显示各屏,
// 用压满边界的假文案检查各分辨率下的排版尺寸是否合适。
//
#ifdef __cplusplus
extern "C" {
#endif

// 0 = 关闭 (默认);>0 = 轮播间隔毫秒。由 main 解析命令行后写入。
extern int g_ui_preview_interval_ms;
// -1 = 不覆盖 (用存档主题);>=0 = 预览强制使用的主题 id。
extern int g_ui_preview_theme_id;
// -1 = 轮播全部;>=0 = 只显示该页面 (ui_preview_screen_parse 的返回值)。
extern int g_ui_preview_screen;

// 解析主题名 (大小写不敏感, 也接受数字下标) -> 主题 id;找不到返回 -1。
int ui_preview_theme_parse(const char *name);
// 解析页面名 (大小写不敏感) -> 预览屏序号;找不到返回 -1。
int ui_preview_screen_parse(const char *name);

// LVGL 线程内调 (screens_init / ui_services_init 之后)。g_ui_preview_interval_ms<=0 时 no-op。
void ui_preview_start(void);

#ifdef __cplusplus
}
#endif
