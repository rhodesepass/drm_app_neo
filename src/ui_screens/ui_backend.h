#pragma once
//
// ui_backend —— 手写 UI 的精简后端 seam (取代 EEZ 的 vars 那一坨)。
//
// 这是 UI 与"数据/业务"之间唯一的注入点：
//   - 设备侧实现接 settings / prts / 电量 等真实子系统；
//   - PC 模拟器由 sim/mock_backend.c 提供桩实现。
// 只放真正需要外部供给的少数钩子，按需扩充，不照搬 FlowGlobalVariables 全表。
//
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *ui_backend_version(void);
int32_t     ui_backend_brightness_get(void);
void        ui_backend_brightness_set(int32_t value);

#ifdef __cplusplus
}
#endif
