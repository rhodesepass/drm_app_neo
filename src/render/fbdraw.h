#pragma once

#include <stdint.h>
#include "lvgl.h"
#include "utils/cacheassets.h"


typedef struct {
    uint32_t* vaddr;
    int width;
    int height;
} fbdraw_fb_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} fbdraw_rect_t;

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