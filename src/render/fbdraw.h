#pragma once
#include "stdint.h"


typedef struct {
    uint32_t *vaddr;
    int fb_width;
    int fb_height;
} fbdraw_t;

void fbdraw_fill_rect(fbdraw_t *fbdraw, int x, int y, int w, int h, int color);
void fbdraw_draw_bitmap_1_bit(fbdraw_t *fbdraw, int x, int y, unsigned char *bitmap, int w, int h, int fgcolor, int bgcolor);
int fbdraw_argb_bitmap_from_file(fbdraw_t *fbdraw, int x,int y,int width,int height, const char *filename);
int fbdraw_argb_bitmap_region_from_file(fbdraw_t *fbdraw, int x,int y,int width,int height, int reg_x,int reg_y,int reg_width,int reg_height, const char *filename);
int fbdraw_argb_bitmap_from_file_with_delay(fbdraw_t *fbdraw, int x,int y,int width,int height, const char *filename,int row_delay);