#include "render/fbdraw.h"
#include "utils/log.h"
#include "utils/stb_image.h"
#include "utils/cacheassets.h"
#include "utils/code128.h"
#include <src/misc/lv_types.h>
#include <string.h>
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/misc/lv_text_private.h"

static inline uint32_t argb8888_blend_over(uint32_t dst, uint8_t src_r, uint8_t src_g, uint8_t src_b, uint8_t src_a)
{
    if(src_a == 0) return dst;
    if(src_a == 255) return (0xFFu << 24) | ((uint32_t)src_r << 16) | ((uint32_t)src_g << 8) | (uint32_t)src_b;

    const uint8_t dst_a = (dst >> 24) & 0xFF;
    const uint8_t dst_r = (dst >> 16) & 0xFF;
    const uint8_t dst_g = (dst >> 8) & 0xFF;
    const uint8_t dst_b = dst & 0xFF;

    const uint32_t inv_sa = 255u - src_a;
    const uint32_t out_a = (uint32_t)src_a + ((uint32_t)dst_a * inv_sa + 127u) / 255u;
    if(out_a == 0) return 0;

    /* 用预乘中间量计算，最终存回“非预乘(straight) ARGB8888” */
    const uint32_t out_r_premul = (uint32_t)src_r * src_a + (((uint32_t)dst_r * dst_a) * inv_sa + 127u) / 255u;
    const uint32_t out_g_premul = (uint32_t)src_g * src_a + (((uint32_t)dst_g * dst_a) * inv_sa + 127u) / 255u;
    const uint32_t out_b_premul = (uint32_t)src_b * src_a + (((uint32_t)dst_b * dst_a) * inv_sa + 127u) / 255u;

    const uint8_t out_r = (uint8_t)((out_r_premul + out_a / 2u) / out_a);
    const uint8_t out_g = (uint8_t)((out_g_premul + out_a / 2u) / out_a);
    const uint8_t out_b = (uint8_t)((out_b_premul + out_a / 2u) / out_a);

    return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

void fbdraw_fill_rect(fbdraw_fb_t* fb, fbdraw_rect_t* rect, uint32_t color){
    for(int y = rect->y; y < rect->y + rect->h; y++){
        for(int x = rect->x; x < rect->x + rect->w; x++){
            fb->vaddr[y * fb->width + x] = color;
        }
    }

}

void fbdraw_copy_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect){
    for(int y = dst_rect->y; y < dst_rect->y + dst_rect->h; y++){
        for(int x = dst_rect->x; x < dst_rect->x + dst_rect->w; x++){
            int src_x = src_rect->x + (x - dst_rect->x);
            int src_y = src_rect->y + (y - dst_rect->y);

            if(src_x >= src_rect->x && src_x < src_rect->x + src_rect->w &&
            src_y >= src_rect->y && src_y < src_rect->y + src_rect->h &&
            x >= 0 && x < dst_fb->width && y >= 0 && y < dst_fb->height &&
            src_x >= 0 && src_x < src_fb->width &&
            src_y >= 0 && src_y < src_fb->height) 
            {
                dst_fb->vaddr[y * dst_fb->width + x] = src_fb->vaddr[src_y * src_fb->width + src_x];
            }
        }
    }
}


void fbdraw_text(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h) {
    uint32_t rgb = color & 0x00FFFFFF;
    const uint8_t color_a = (color >> 24) & 0xFF;
    if (line_h <= 0) {
        line_h = (int32_t)lv_font_get_line_height(font);
    }
    const int32_t x0 = rect->x;
    int32_t cursor_x = rect->x;
    int32_t cursor_y = rect->y;

    uint32_t ofs = 0;
    uint32_t codepoint = 0;
    while((codepoint = lv_text_encoded_next(text, &ofs)) != 0) {
        if(codepoint == '\n') {
            cursor_x = x0;
            cursor_y += line_h;
            continue;
        }
        if(codepoint == '\r') {
            cursor_x = x0;
            continue;
        }

        uint32_t codepoint_next = lv_text_encoded_next(&text[ofs], NULL);

        lv_font_glyph_dsc_t g_dsc;
        if(!lv_font_get_glyph_dsc(font, &g_dsc, codepoint, codepoint_next)) {
            /* 字符不可用，跳过 */
            continue;
        }

        /* 空白字符等无需绘制 */
        if(g_dsc.box_w == 0 || g_dsc.box_h == 0) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
            continue;
        }

        lv_draw_buf_t * glyph_draw_buf = lv_draw_buf_create_ex(lv_draw_buf_get_font_handlers(),
                                                               g_dsc.box_w, g_dsc.box_h,
                                                               LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
        if(!glyph_draw_buf) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
            continue;
        }

        g_dsc.req_raw_bitmap = 0;
        const lv_draw_buf_t * glyph_buf = (const lv_draw_buf_t *)lv_font_get_glyph_bitmap(&g_dsc, glyph_draw_buf);

        if(glyph_buf && glyph_buf->data && glyph_buf->header.cf == LV_COLOR_FORMAT_A8) {
            const uint8_t * a8 = (const uint8_t *)glyph_buf->data;
            const uint32_t stride = glyph_buf->header.stride;

            /* 参照 LVGL label 的基线计算：y 视为“行顶部” */
            const int base_y = (int)cursor_y + (int)(font->line_height - font->base_line);
            const uint8_t src_r = (rgb >> 16) & 0xFF;
            const uint8_t src_g = (rgb >> 8) & 0xFF;
            const uint8_t src_b = rgb & 0xFF;

            for(int row = 0; row < (int)g_dsc.box_h; ++row) {
                const uint8_t * a8_row = a8 + row * stride;
                for(int col = 0; col < (int)g_dsc.box_w; ++col) {
                    uint8_t pixel_alpha = a8_row[col];
                    if(pixel_alpha == 0) continue;
                    if(color_a != 255) pixel_alpha = (uint8_t)(((uint32_t)pixel_alpha * color_a + 127u) / 255u);

                    const int px = (int)cursor_x + (int)g_dsc.ofs_x + col;
                    const int py = base_y - (int)g_dsc.box_h - (int)g_dsc.ofs_y + row;
                    if(px < rect->x || px >= rect->x + rect->w || py < rect->y || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    uint32_t * dst = fb->vaddr + px + py * fb->width;
                    *dst = argb8888_blend_over(*dst, src_r, src_g, src_b, pixel_alpha);
                }
            }
        }

        lv_font_glyph_release_draw_data(&g_dsc);
        lv_draw_buf_destroy(glyph_draw_buf);

        /* glyph advance（含 kerning） */
        cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
    }
}

void fbdraw_text_vertical(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color) {
    uint32_t rgb = color & 0x00FFFFFF;
    const uint8_t color_a = (color >> 24) & 0xFF;
    int32_t line_height = (int32_t)lv_font_get_line_height(font);
    int32_t cursor_y = rect->y;

    uint32_t ofs = 0;
    uint32_t codepoint = 0;
    while((codepoint = lv_text_encoded_next(text, &ofs)) != 0) {
        if(codepoint == '\n' || codepoint == '\r') continue;

        uint32_t codepoint_next = lv_text_encoded_next(&text[ofs], NULL);

        lv_font_glyph_dsc_t g_dsc;
        if(!lv_font_get_glyph_dsc(font, &g_dsc, codepoint, codepoint_next)) continue;

        if(g_dsc.box_w == 0 || g_dsc.box_h == 0) {
            cursor_y += line_height;
            continue;
        }

        /* 水平居中 */
        int32_t glyph_w = (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
        int32_t cursor_x = rect->x + (rect->w - glyph_w) / 2;

        lv_draw_buf_t * glyph_draw_buf = lv_draw_buf_create_ex(
            lv_draw_buf_get_font_handlers(),
            g_dsc.box_w, g_dsc.box_h,
            LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
        if(!glyph_draw_buf) {
            cursor_y += line_height;
            continue;
        }

        g_dsc.req_raw_bitmap = 0;
        const lv_draw_buf_t * glyph_buf = (const lv_draw_buf_t *)lv_font_get_glyph_bitmap(&g_dsc, glyph_draw_buf);

        if(glyph_buf && glyph_buf->data && glyph_buf->header.cf == LV_COLOR_FORMAT_A8) {
            const uint8_t * a8 = (const uint8_t *)glyph_buf->data;
            const uint32_t stride = glyph_buf->header.stride;
            const int base_y = (int)cursor_y + (int)(font->line_height - font->base_line);
            const uint8_t src_r = (rgb >> 16) & 0xFF;
            const uint8_t src_g = (rgb >> 8) & 0xFF;
            const uint8_t src_b = rgb & 0xFF;

            for(int row = 0; row < (int)g_dsc.box_h; ++row) {
                const uint8_t * a8_row = a8 + row * stride;
                for(int col = 0; col < (int)g_dsc.box_w; ++col) {
                    uint8_t pixel_alpha = a8_row[col];
                    if(pixel_alpha == 0) continue;
                    if(color_a != 255) pixel_alpha = (uint8_t)(((uint32_t)pixel_alpha * color_a + 127u) / 255u);

                    const int px = (int)cursor_x + (int)g_dsc.ofs_x + col;
                    const int py = base_y - (int)g_dsc.box_h - (int)g_dsc.ofs_y + row;
                    if(px < rect->x || px >= rect->x + rect->w || py < rect->y || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    uint32_t * dst = fb->vaddr + px + py * fb->width;
                    *dst = argb8888_blend_over(*dst, src_r, src_g, src_b, pixel_alpha);
                }
            }
        }

        lv_font_glyph_release_draw_data(&g_dsc);
        lv_draw_buf_destroy(glyph_draw_buf);

        cursor_y += line_height;
    }
}

void fbdraw_text_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect,
                       const char* text, const lv_font_t* font, uint32_t color) {
    // 1. 分配临时缓冲，宽高互换（与 fbdraw_barcode_rot90 相同模式）
    int buf_w = rect->h;
    int buf_h = rect->w;
    uint32_t* buf = calloc(buf_w * buf_h, sizeof(uint32_t));
    if (!buf) return;

    // 2. 在临时缓冲中水平渲染文字
    fbdraw_fb_t tmp_fb = { .vaddr = buf, .width = buf_w, .height = buf_h };
    fbdraw_rect_t tmp_rect = { .x = 0, .y = 0, .w = buf_w, .h = buf_h };
    fbdraw_text(&tmp_fb, &tmp_rect, text, font, color, 0);

    // 3. 顺时针旋转 +90° (CW) 并写入目标
    for (int y = rect->y; y < rect->y + rect->h; y++) {
        for (int x = rect->x; x < rect->x + rect->w; x++) {
            int local_x = y - rect->y;
            int local_y = (rect->w - 1) - (x - rect->x);
            if (local_x < 0 || local_x >= buf_w || local_y < 0 || local_y >= buf_h) continue;
            uint32_t pixel = buf[local_y * buf_w + local_x];
            if (((pixel >> 24) & 0xFF) == 0) continue;
            fb->vaddr[y * fb->width + x] = pixel;
        }
    }

    free(buf);
}

void fbdraw_text_range(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h,int start_cp,int end_cp) {
    uint32_t rgb = color & 0x00FFFFFF;
    const uint8_t color_a = (color >> 24) & 0xFF;
    if (line_h <= 0) {
        line_h = (int32_t)lv_font_get_line_height(font);
    }
    const int32_t x0 = rect->x;
    int32_t cursor_x = rect->x;
    int32_t cursor_y = rect->y;

    uint32_t ofs = 0;
    uint32_t codepoint = 0;
    int cp_idx = 0;
    // log_trace("range into");
    while((codepoint = lv_text_encoded_next(text, &ofs)) != 0) {
        if(codepoint == '\n') {
            cursor_x = x0;
            cursor_y += line_h;
            continue;
        }
        if(codepoint == '\r') {
            cursor_x = x0;
            continue;
        }

        uint32_t codepoint_next = lv_text_encoded_next(&text[ofs], NULL);
        cp_idx++;
        if(cp_idx < start_cp) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
            continue;
        }
        if(cp_idx >= end_cp) break;

        // log_trace("cp_idx=%d,codepoint=%d,codepoint_next=%d", cp_idx, codepoint, codepoint_next);

        lv_font_glyph_dsc_t g_dsc;
        if(!lv_font_get_glyph_dsc(font, &g_dsc, codepoint, codepoint_next)) {
            /* 字符不可用，跳过 */
            continue;
        }

        /* 空白字符等无需绘制 */
        if(g_dsc.box_w == 0 || g_dsc.box_h == 0) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
            continue;
        }

        lv_draw_buf_t * glyph_draw_buf = lv_draw_buf_create_ex(lv_draw_buf_get_font_handlers(),
                                                               g_dsc.box_w, g_dsc.box_h,
                                                               LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
        if(!glyph_draw_buf) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
            continue;
        }

        g_dsc.req_raw_bitmap = 0;
        const lv_draw_buf_t * glyph_buf = (const lv_draw_buf_t *)lv_font_get_glyph_bitmap(&g_dsc, glyph_draw_buf);

        if(glyph_buf && glyph_buf->data && glyph_buf->header.cf == LV_COLOR_FORMAT_A8) {
            const uint8_t * a8 = (const uint8_t *)glyph_buf->data;
            const uint32_t stride = glyph_buf->header.stride;

            /* 参照 LVGL label 的基线计算：y 视为“行顶部” */
            const int base_y = (int)cursor_y + (int)(font->line_height - font->base_line);
            const uint8_t src_r = (rgb >> 16) & 0xFF;
            const uint8_t src_g = (rgb >> 8) & 0xFF;
            const uint8_t src_b = rgb & 0xFF;

            for(int row = 0; row < (int)g_dsc.box_h; ++row) {
                const uint8_t * a8_row = a8 + row * stride;
                for(int col = 0; col < (int)g_dsc.box_w; ++col) {
                    uint8_t pixel_alpha = a8_row[col];
                    if(pixel_alpha == 0) continue;
                    if(color_a != 255) pixel_alpha = (uint8_t)(((uint32_t)pixel_alpha * color_a + 127u) / 255u);

                    const int px = (int)cursor_x + (int)g_dsc.ofs_x + col;
                    const int py = base_y - (int)g_dsc.box_h - (int)g_dsc.ofs_y + row;
                    if(px < rect->x || px >= rect->x + rect->w || py < rect->y || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    uint32_t * dst = fb->vaddr + px + py * fb->width;
                    *dst = argb8888_blend_over(*dst, src_r, src_g, src_b, pixel_alpha);
                }
            }
        }

        lv_font_glyph_release_draw_data(&g_dsc);
        lv_draw_buf_destroy(glyph_draw_buf);

        /* glyph advance（含 kerning） */
        cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
    }
    // log_trace("range out");

}

void fbdraw_image(fbdraw_fb_t* fb, fbdraw_rect_t* rect, char* image_path){
    int w,h,c;
    uint8_t* pixdata = stbi_load(image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", image_path);
        return;
    }

    for(int y = rect->y; y < rect->y + rect->h; y++){
        for(int x = rect->x; x < rect->x + rect->w; x++){

            int src_x = x - rect->x;
            int src_y = y - rect->y;

            if(src_x >= 0 && src_x < w && src_y >= 0 && src_y < h){
                uint32_t * dst = fb->vaddr + x + y * fb->width;
                uint32_t bgra_pixel = *((uint32_t *)(pixdata) + src_x + src_y * w);
                uint32_t rgb_pixel = (bgra_pixel & 0x000000FF) << 16 | (bgra_pixel & 0x0000FF00) | (bgra_pixel & 0x00FF0000) >> 16 | (bgra_pixel & 0xFF000000);
                *dst = rgb_pixel;
            }
        }
    }

    stbi_image_free(pixdata);
}

void fbdraw_cacheassets(fbdraw_fb_t* fb,fbdraw_rect_t* rect, cacheasset_asset_id_t assetid){
    int w,h;
    uint8_t* pixdata;
    cacheassets_get_asset_from_global(assetid, &w, &h, &pixdata);
    if(!pixdata){
        log_error("failed to get asset: %d", assetid);
        return;
    }


    for(int y = rect->y; y < rect->y + rect->h; y++){
        for(int x = rect->x; x < rect->x + rect->w; x++){
            int src_x = x - rect->x;
            int src_y = y - rect->y;

            if(src_x >= 0 && src_x < w && src_y >= 0 && src_y < h){
                uint32_t * dst = fb->vaddr + x + y * fb->width;
                *dst = *((uint32_t *)(pixdata) + src_x + src_y * w);
            }
        }
    }
}

// 将src_fb内的src_rect ，在它的alpha的基础上，乘 opacity / 255 ，再混合到dst_fb内的dst_rect中。
void fbdraw_alpha_opacity_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect,uint8_t opacity){
    if(!src_fb || !dst_fb || !src_rect || !dst_rect) return;
    if(!src_fb->vaddr || !dst_fb->vaddr) return;
    if(opacity == 0) return;
    if(dst_rect->w <= 0 || dst_rect->h <= 0 || src_rect->w <= 0 || src_rect->h <= 0) return;

    for(int y = dst_rect->y; y < dst_rect->y + dst_rect->h; y++){
        if(y < 0 || y >= dst_fb->height) continue;
        for(int x = dst_rect->x; x < dst_rect->x + dst_rect->w; x++){
            if(x < 0 || x >= dst_fb->width) continue;

            const int src_x = src_rect->x + (x - dst_rect->x);
            const int src_y = src_rect->y + (y - dst_rect->y);

            if(src_x < src_rect->x || src_x >= src_rect->x + src_rect->w ||
               src_y < src_rect->y || src_y >= src_rect->y + src_rect->h) {
                continue;
            }
            if(src_x < 0 || src_x >= src_fb->width || src_y < 0 || src_y >= src_fb->height) continue;

            const uint32_t src_px = src_fb->vaddr[src_y * src_fb->width + src_x];
            const uint8_t sa = (src_px >> 24) & 0xFF;
            if(sa == 0) continue;

            uint8_t a = sa;
            if(opacity != 255) a = (uint8_t)(((uint32_t)sa * opacity + 127u) / 255u);
            if(a == 0) continue;

            const uint8_t r = (src_px >> 16) & 0xFF;
            const uint8_t g = (src_px >> 8) & 0xFF;
            const uint8_t b = src_px & 0xFF;

            uint32_t * dst = dst_fb->vaddr + y * dst_fb->width + x;
            *dst = argb8888_blend_over(*dst, r, g, b, a);
        }
    }
}

void fbdraw_barcode_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* str, const lv_font_t* font){

    int buf_w = rect->h;
    int buf_h = rect->w;

    uint32_t* buf = malloc(buf_w * buf_h * sizeof(uint32_t));
    for(int i = 0;i < buf_w * buf_h;i++){
        buf[i] = 0xFFFFFFFF;
    }

    int font_height = lv_font_get_line_height(font);
    int barcode_height = buf_h - font_height;

    if(barcode_height<0) return;

    int barcode_length = code128_estimate_len(str);
    char *barcode_data = (char *) malloc(barcode_length);

    barcode_length = code128_encode_gs1(str, barcode_data, barcode_length);

    /* barcode_length is now the actual number of "bars". */
    int first_bar_index = 0;
    int first_bar_occured = 0;
    for (int i = 0; i < barcode_length; i++) {
        int bar_index = i - first_bar_index;
        if(bar_index < 0) continue;
        if(bar_index > (buf_w / 2)) break;
        if (barcode_data[i]){
            if (!first_bar_occured){
                first_bar_index = i;
                first_bar_occured = 1;
            }
            for(int y = 0;y < barcode_height;y++){
                buf[y * buf_w + bar_index*2] = 0xFF000000;
                buf[y * buf_w + bar_index*2+1] = 0xFF000000;
            }
        }
    }

    fbdraw_fb_t fbdst;
    fbdst.vaddr = buf;
    fbdst.width = buf_w;
    fbdst.height = buf_h;

    fbdraw_rect_t dst_rect;
    dst_rect.x = 0;
    dst_rect.y = barcode_height;
    dst_rect.w = buf_w;
    dst_rect.h = font_height;

    fbdraw_text(&fbdst, &dst_rect, str, font, 0xFF000000, 0);

    for(int y = rect->y; y < rect->y + rect->h; y++){
        for(int x = rect->x; x < rect->x + rect->w; x++){
            // Rotate the barcode and text buffer -90 degrees (ccw) and blit it into destination framebuffer at rect
            // The buffer buf is [rect->w x rect->h], but is to be output -90deg rotated into fb->vaddr
            // (x', y') in destination corresponds to (rect->h - 1 - y, x) in the buf
            int local_x = (rect->h - 1) - (y - rect->y);
            int local_y = x - rect->x;
            if (local_x >= 0 && local_x < buf_w && local_y >= 0 && local_y < buf_h) {
                uint32_t pixel = buf[local_y * buf_w + local_x];
                fb->vaddr[y * fb->width + x] = pixel;
            }
        }
    }

    free(buf);
    free(barcode_data);
}