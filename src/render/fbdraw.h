#pragma once

#include <stdint.h>
#include "lvgl.h"
#include "config.h"
#include "utils/cacheassets.h"
#include "render/c8pal.h"


typedef enum {
    FBDRAW_FMT_ARGB8888 = 0,   // 0 值即默认: {0} / designated 初始化天然落到 8888
    FBDRAW_FMT_C8 = 1,         // 调色板索引, 1B/px, 经 c8pal 反查/展开
} fbdraw_fmt_t;

// overlay 显存(及 cacheasset 素材)的编译期格式, 由 config.h 的开关决定
#if OVERLAY_USE_C8
#define FBDRAW_OVERLAY_FMT FBDRAW_FMT_C8
#else
#define FBDRAW_OVERLAY_FMT FBDRAW_FMT_ARGB8888
#endif

typedef struct {
    uint32_t* vaddr;    // fmt=C8 时实际按 uint8_t* 使用
    int width;
    int height;
    fbdraw_fmt_t fmt;
} fbdraw_fb_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} fbdraw_rect_t;

// 8x8 Bayer(64 级)有序抖动矩阵。按屏幕坐标索引,同一像素每次结果相同,
// 静态内容帧间逐位稳定不闪;渐变表现为 stipple 密度渐变
static const uint8_t fbdraw_bayer8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42},
    { 48, 16, 56, 24, 50, 18, 58, 26},
    { 12, 44,  4, 36, 14, 46,  6, 38},
    { 60, 28, 52, 20, 62, 30, 54, 22},
    {  3, 35, 11, 43,  1, 33,  9, 41},
    { 51, 19, 59, 27, 49, 17, 57, 25},
    { 15, 47,  7, 39, 13, 45,  5, 37},
    { 63, 31, 55, 23, 61, 29, 53, 21},
};

// straight ARGB8888 的 src-over 混合。
// dst 可能指向 uncached 显存：读回代价远高于写，
// 只有半透明像素才回读，opaque 直接写、全透明直接跳过。
static inline void fbdraw_blend_over_at(uint32_t* dst_p, uint8_t src_r, uint8_t src_g, uint8_t src_b, uint8_t src_a)
{
    if(src_a == 0) return;
    if(src_a == 255) {
        *dst_p = (0xFFu << 24) | ((uint32_t)src_r << 16) | ((uint32_t)src_g << 8) | (uint32_t)src_b;
        return;
    }

    const uint32_t dst = *dst_p;
    const uint8_t dst_a = (dst >> 24) & 0xFF;
    const uint8_t dst_r = (dst >> 16) & 0xFF;
    const uint8_t dst_g = (dst >> 8) & 0xFF;
    const uint8_t dst_b = dst & 0xFF;

    const uint32_t inv_sa = 255u - src_a;
    const uint32_t out_a = (uint32_t)src_a + ((uint32_t)dst_a * inv_sa + 127u) / 255u;
    if(out_a == 0) {
        *dst_p = 0;
        return;
    }

    /* 用预乘中间量计算，最终存回“非预乘(straight) ARGB8888” */
    const uint32_t out_r_premul = (uint32_t)src_r * src_a + (((uint32_t)dst_r * dst_a) * inv_sa + 127u) / 255u;
    const uint32_t out_g_premul = (uint32_t)src_g * src_a + (((uint32_t)dst_g * dst_a) * inv_sa + 127u) / 255u;
    const uint32_t out_b_premul = (uint32_t)src_b * src_a + (((uint32_t)dst_b * dst_a) * inv_sa + 127u) / 255u;

    const uint8_t out_r = (uint8_t)((out_r_premul + out_a / 2u) / out_a);
    const uint8_t out_g = (uint8_t)((out_g_premul + out_a / 2u) / out_a);
    const uint8_t out_b = (uint8_t)((out_b_premul + out_a / 2u) / out_a);

    *dst_p = ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

// 按 fb->fmt 读一个像素，统一返回 ARGB8888(C8 经调色板展开)
static inline uint32_t fbdraw_read_px(const fbdraw_fb_t* fb, int x, int y)
{
    if(fb->fmt == FBDRAW_FMT_C8)
        return c8pal_color(((const uint8_t*)fb->vaddr)[(size_t)y * fb->width + x]);
    return fb->vaddr[(size_t)y * fb->width + x];
}

// 按 fb->fmt 直写一个像素(覆盖语义, 不混合; C8 经反查 LUT 落最近索引)
static inline void fbdraw_write_px(fbdraw_fb_t* fb, int x, int y, uint32_t argb)
{
    if(fb->fmt == FBDRAW_FMT_C8)
        ((uint8_t*)fb->vaddr)[(size_t)y * fb->width + x] = c8pal_index(argb);
    else
        fb->vaddr[(size_t)y * fb->width + x] = argb;
}

// 按 fb->fmt 分派的逐像素 src-over(字体/图片的落点统一走这里)。
// C8: 底透明或不透明都先展开成 8888 混合, 结果反查回索引——半透明结果
// (文字 AA 落透明底等)靠调色板里的 alpha ramp 项吸收。
static inline void fbdraw_blend_px(fbdraw_fb_t* fb, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if(fb->fmt == FBDRAW_FMT_C8){
        if(a == 0) return;
        uint8_t* dst = (uint8_t*)fb->vaddr + (size_t)y * fb->width + x;
        if(a == 255){
            *dst = c8pal_index(0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
            return;
        }
        uint32_t tmp = c8pal_color(*dst);
        fbdraw_blend_over_at(&tmp, r, g, b, a);
        *dst = c8pal_index(tmp);
    } else {
        fbdraw_blend_over_at(fb->vaddr + (size_t)y * fb->width + x, r, g, b, a);
    }
}


int32_t fbdraw_text_width(const char* text, const lv_font_t* font, int32_t letter_space);
void fbdraw_fill_rect(fbdraw_fb_t* fb, fbdraw_rect_t* rect, uint32_t color);
void fbdraw_copy_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect);
void fbdraw_text(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h,int32_t letter_space);
void fbdraw_text_vertical(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color);
void fbdraw_text_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t letter_space);
void fbdraw_text_range(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h,int start_cp,int end_cp);

void fbdraw_image(fbdraw_fb_t* fb, fbdraw_rect_t* rect, char* image_path);
void fbdraw_cacheassets(fbdraw_fb_t* fb,fbdraw_rect_t* rect, cacheasset_asset_id_t assetid);

void fbdraw_alpha_opacity_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect,uint8_t opacity);
void fbdraw_barcode_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* str,const lv_font_t* font);
