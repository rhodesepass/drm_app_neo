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


void fbdraw_fill_rect(fbdraw_fb_t* fb, fbdraw_rect_t* rect, uint32_t color);
void fbdraw_copy_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect);
void fbdraw_text(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h);
void fbdraw_text_vertical(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color);
void fbdraw_text_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color);
void fbdraw_text_range(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h,int start_cp,int end_cp);

void fbdraw_image(fbdraw_fb_t* fb, fbdraw_rect_t* rect, char* image_path);
void fbdraw_cacheassets(fbdraw_fb_t* fb,fbdraw_rect_t* rect, cacheasset_asset_id_t assetid);

void fbdraw_alpha_opacity_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect,uint8_t opacity);
void fbdraw_barcode_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* str,const lv_font_t* font);