#pragma once
// C8(256 色调色板)管理器 —— overlay 层 palette 模式的唯一真相源。
//
// shadow 表(uint32 ARGB8888 x256) + 反查 LUT + median-cut 量化 + 磁盘缓存。
// 分段布局见 config.h 的 C8PAL_* 宏。所有权模型:动态段不做全局分配器,
// "谁下一个画,谁在层离屏时重写自己的子段并 commit"。commit 立即锁存不等
// vsync,必须只在 overlay 静止点(abort 后/show 绘制前)调用——overlay 单
// plane + 绘制前都有 overlay_abort() 串行化,该不变量由调用方保证。
//
// 线程:write_*/commit 只在 show/transition 入口线程调用;c8pal_index/color
// 在 worker 绘制线程调用(lazy 填 LUT)。两者不重叠(commit 时 worker 未跑)。

#include <stdint.h>
#include "driver/drm_warpper.h"

// 量化/索引缓存文件布局版本,分段配额变更时递增强制重算
#define C8PAL_CACHE_VERSION 1

void c8pal_init(drm_warpper_t* dw);

// 把烘焙段 0..C8PAL_BAKED_COUNT-1 恢复进 shadow(image 模式会整表覆盖,
// 其余 owner 在写自己动态子段前先调这个)
void c8pal_restore_baked(void);

// 索引 -> ARGB8888(读 shadow 表)
uint32_t c8pal_color(uint8_t idx);

// ARGB8888 -> 最近索引(双 LUT lazy 填充; a<8 直接落透明项)
uint8_t c8pal_index(uint32_t argb);

// 精确匹配:shadow 表里找这个颜色(逐位相等),没有返回 -1。
// 给"已知颜色一定在表里"的专用路径用(如 theme ramp 层级定位),
// 绕过 LUT 的 alpha 16 桶分辨率限制
int c8pal_find_exact(uint32_t argb);

// 写动态子段(只改 shadow,不上传)
void c8pal_write_range(int base, const uint32_t* colors, int n);

// 上传整表 1024B 到硬件(sim: 软副本) + 清空反查 LUT
void c8pal_commit(void);

// 带磁盘缓存的量化:对 px(w*h, ARGB8888) 做 median-cut 聚 max_n 色 +
// Floyd-Steinberg 抖动,**就地改写 px** 为量化后的展开像素(每像素都是
// out_pal 中的精确色,落盘 LUT 反查时无损往返)。缓存 <img_path>.c8pal /
// .c8i 与源图同目录(素材包可写;res/ 只读时写失败静默降级现算)。
// 返回实际色数,<0 失败(px 未动,调用方按原图走)。
int c8pal_load_or_quantize(const char* img_path, uint32_t* px, int w, int h,
                           int max_n, uint32_t* out_pal);

// ---- 颜色池:owner(opinfo/transition) 在 load 阶段攒动态段颜色 ----
// 追加去重;池满丢弃(LUT 反查会落到最近色,只降质不出错)
void c8pal_pool_add(uint32_t* pool, int* n, int cap, const uint32_t* colors, int cnt);
// opaque 本色 + levels 级 alpha 渐变(文字 AA/corner fade 的半透明落点)
void c8pal_pool_add_ramp(uint32_t* pool, int* n, int cap, uint32_t color, int levels);
