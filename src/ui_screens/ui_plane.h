#pragma once
//
// ui_plane —— UI 平面滑动过渡的平台 seam。
//
// 真正的屏间过渡是"整块 UI 图层在面板上滑动 Y"(sunxi DEBE 图层坐标能力)，
// 不是 LVGL 的事。screen_manager 负责"滑到哪、何时滑"的策略，
// 具体怎么滑由各平台实现这个接口：
//   - 设备侧: layer_animation_ease_in_out_move(DRM_WARPPER_LAYER_UI, ...)
//   - PC 模拟器: 无多 DRM 平面，当前实现为 no-op (维持 LVGL 满屏淡入)
//
// 单位与设备侧 layer_animation 对齐，用微秒。
//
#ifdef __cplusplus
extern "C" {
#endif

void ui_plane_move(int from_y, int to_y, int duration_us, int delay_us);

#ifdef __cplusplus
}
#endif
