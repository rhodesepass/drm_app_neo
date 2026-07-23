#pragma once
//
// ui_preview —— 调试用弹窗轮播预览 (仅 PC/sim 侧有意义)。
//
// 命令行传 `preview [间隔ms]` 时,main 设置 g_ui_preview_interval_ms,LVGL 初始化
// 完成后 ui_preview_start() 建一个 lv_timer,依次强制弹出
// warning / confirm(大字) / fido-uix confirm(小字) / usbselect 并循环,
// 用压满边界的假文案检查各分辨率下的排版尺寸是否合适。
//
#ifdef __cplusplus
extern "C" {
#endif

// 0 = 关闭 (默认);>0 = 轮播间隔毫秒。由 main 解析命令行后写入。
extern int g_ui_preview_interval_ms;

// LVGL 线程内调 (screens_init / ui_services_init 之后)。g_ui_preview_interval_ms<=0 时 no-op。
void ui_preview_start(void);

#ifdef __cplusplus
}
#endif
