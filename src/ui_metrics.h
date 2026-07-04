#pragma once
//
// ui_metrics.h —— 分辨率适配的单一真值源
//
// 设计基准为 360x640。两个硬件目标正好整数 2x、同为 9:16：
//   F1C200s 360x640  -> UI_SCALE = 1
//   T113    720x1280 -> UI_SCALE = 2
// UI_SCALE 由 config.h 的屏选宏 (USE_360_640_SCREEN / USE_720_1280_SCREEN) 推导。
//
// 所有手写 UI 的 pos / size / radius / pad / 字号 一律以 360 基准书写，外面套 S()，
// 整数倍缩放无小数定位糊边。overlay / video plane 的坐标后续也统一走 S()。
//
#include "config.h"

#ifndef UI_SCALE
#error "UI_SCALE not defined — select a screen target (USE_360_640_SCREEN / USE_720_1280_SCREEN) in config.h"
#endif

// 把基准设计单位 (360x640 下的像素) 换算到当前目标
#define S(x) ((x) * UI_SCALE)

// 设计基准尺寸 (不随目标变化，需要乘 S() 才得到实际像素)
#define UI_BASE_WIDTH  360
#define UI_BASE_HEIGHT 640

// 安全边距 (基准单位)
#define UI_SAFE_MARGIN 8
