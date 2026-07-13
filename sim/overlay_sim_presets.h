#pragma once
//
// overlay 仿真的干员预设表 —— 每个预设填充一份 olopinfo_params_t（含元素列表），
// 覆盖元素引擎的全部元素类型与动画组合。
//
#include "overlay/opinfo.h"

#define OVERLAY_SIM_PRESET_COUNT 5

const char* overlay_sim_preset_name(int idx);

// 填充 params（内部会分配 elements，调用前须先 overlay_opinfo_free_elements 旧的）。
// 成功返回 0。
int overlay_sim_preset_build(int idx, olopinfo_params_t* params);
