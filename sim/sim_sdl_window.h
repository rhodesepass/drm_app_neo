#pragma once
//
// sim 自含 SDL 后端 —— 从 vendored lv_sdl_window 裁剪而来，固定本工程配置
// (RGB565 / DIRECT / 单缓冲 / 不旋转 / 不缩放)，并额外支持把整块 UI 纹理贴到
// 带 Y 偏移的 dst 矩形 (viewport)，用来在 PC 上模拟设备 DEBE 图层在面板上滑动的
// 屏间过渡。露出的区域填黑，对应设备上 UI 层下方的立绘背景层。
//
// 与 lv_sdl_window 不同，这里输入也自己拥有 (encoder indev + 物理键回调)，
// 不复用 vendored 的键鼠 handler，故不耦合 lv_sdl_window_t 的私有结构布局。
//
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 建窗 + renderer + 全屏纹理 + LVGL 显示 (DIRECT 单缓冲) + encoder indev + SDL 事件泵。
// 窗口位置可用环境变量 SIM_WIN_X / SIM_WIN_Y 指定 (否则交给窗口管理器)。
lv_display_t *sim_window_create(int32_t hor_res, int32_t ver_res);

// 喂 group 用的 encoder 输入设备 (屏内焦点/编辑)。导航(切屏)另走 sim_window_set_key_cb。
// 注: 设备侧也是 ENCODER (key_enc_evdev)，左/右键靠 encoder 处理移焦点。
lv_indev_t *sim_window_indev(void);

// UI 平面在窗口里的 Y 偏移 (viewport)。
void sim_window_set_plane_y(int y);
int  sim_window_get_plane_y(void);

// 用当前偏移立即重贴一帧。动画期间内容未变，LVGL 不会重 flush，须手动推帧。
void sim_window_present(void);

// 物理按键回调 (对应设备侧 key_enc_evdev.input_cb)，每次按下边沿触发一次。
typedef void (*sim_key_cb_t)(uint32_t lv_key);
void sim_window_set_key_cb(sim_key_cb_t cb);

#ifdef __cplusplus
}
#endif
