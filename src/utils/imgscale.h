#pragma once

#include <stdint.h>

// 最近邻整数倍放大 RGBA8888 图像(用于 720p 档显示 360 基准旧素材)。
// 就地替换:成功时 free 旧 buffer,回写 *pixels/*w/*h;失败保留原图返 -1。
// factor <= 1 直接返回 0。
int imgscale_upscale_nn_rgba(uint32_t **pixels, int *w, int *h, int factor);

// 最近邻整数倍缩小(用于 360 档显示 720 基准素材)。语义/所有权同上。
// factor <= 1 直接返回 0；缩小后尺寸为 0 返 -1 保留原图。
int imgscale_downscale_nn_rgba(uint32_t **pixels, int *w, int *h, int factor);

// 先放大 up 倍再缩小 down 倍(实际只会有一侧 > 1)。任一步失败即返回其错误码。
int imgscale_rescale_nn_rgba(uint32_t **pixels, int *w, int *h, int up, int down);
