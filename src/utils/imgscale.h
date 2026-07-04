#pragma once

#include <stdint.h>

// 最近邻整数倍放大 RGBA8888 图像(用于 720p 档显示 360 基准旧素材)。
// 就地替换:成功时 free 旧 buffer,回写 *pixels/*w/*h;失败保留原图返 -1。
// factor <= 1 直接返回 0。
int imgscale_upscale_nn_rgba(uint32_t **pixels, int *w, int *h, int factor);
