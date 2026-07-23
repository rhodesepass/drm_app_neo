#include "render/fbdraw.h"
#include "utils/log.h"
#include "utils/stb_image.h"
#include "utils/cacheassets.h"
#include "utils/code128.h"
#include <src/misc/lv_types.h>
#include <string.h>
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/misc/lv_text_private.h"
#include "ui_metrics.h"


void fbdraw_fill_rect(fbdraw_fb_t* fb, fbdraw_rect_t* rect, uint32_t color){
    int x = rect->x;
    int y = rect->y;
    int w = rect->w;
    int h = rect->h;

    if(x < 0){ w += x; x = 0; }
    if(y < 0){ h += y; y = 0; }
    if(x + w > fb->width) w = fb->width - x;
    if(y + h > fb->height) h = fb->height - y;
    if(w <= 0 || h <= 0) return;

    if(fb->fmt == FBDRAW_FMT_C8){
        // 纯色 = 单索引, 1B/px memset(填充带宽是 8888 的 1/4)
        uint8_t idx = c8pal_index(color);
        uint8_t* dst8 = (uint8_t*)fb->vaddr + (size_t)y * fb->width + x;
        for(int j = 0; j < h; j++){
            memset(dst8, idx, (size_t)w);
            dst8 += fb->width;
        }
        return;
    }

    uint32_t* dst = fb->vaddr + y * fb->width + x;

    // 显存是 uncached/写合并映射，逐像素 str 吃不满总线；
    // 四字节相同的颜色直接走 memset，其余先在栈上(cached)铺一行模板，
    // 再整行 memcpy，让 libc 用 ldm/stm 突发写。
    const uint8_t c0 = color & 0xFF;
    if(c0 == ((color >> 8) & 0xFF) && c0 == ((color >> 16) & 0xFF) && c0 == (color >> 24)){
        for(int j = 0; j < h; j++){
            memset(dst, c0, (size_t)w * 4);
            dst += fb->width;
        }
        return;
    }

    uint32_t row[w];
    for(int i = 0; i < w; i++) row[i] = color;
    for(int j = 0; j < h; j++){
        memcpy(dst, row, (size_t)w * 4);
        dst += fb->width;
    }
}

void fbdraw_copy_rect(fbdraw_fb_t* src_fb, fbdraw_fb_t* dst_fb, fbdraw_rect_t* src_rect, fbdraw_rect_t* dst_rect){
    int w = src_rect->w < dst_rect->w ? src_rect->w : dst_rect->w;
    int h = src_rect->h < dst_rect->h ? src_rect->h : dst_rect->h;
    int sx = src_rect->x;
    int sy = src_rect->y;
    int dx = dst_rect->x;
    int dy = dst_rect->y;

    // 任一侧起点越界时，两侧起点同步平移，保持 src/dst 像素对应关系
    int d;
    if((d = -sx) > 0){ sx += d; dx += d; w -= d; }
    if((d = -dx) > 0){ sx += d; dx += d; w -= d; }
    if((d = -sy) > 0){ sy += d; dy += d; h -= d; }
    if((d = -dy) > 0){ sy += d; dy += d; h -= d; }
    if((d = sx + w - src_fb->width) > 0) w -= d;
    if((d = dx + w - dst_fb->width) > 0) w -= d;
    if((d = sy + h - src_fb->height) > 0) h -= d;
    if((d = dy + h - dst_fb->height) > 0) h -= d;
    if(w <= 0 || h <= 0) return;

    if(src_fb->fmt == FBDRAW_FMT_C8 && dst_fb->fmt == FBDRAW_FMT_C8){
        // 同为索引(cacheasset -> VRAM): 行 memcpy, 1B/px
        const uint8_t* src8 = (const uint8_t*)src_fb->vaddr + (size_t)sy * src_fb->width + sx;
        uint8_t* dst8 = (uint8_t*)dst_fb->vaddr + (size_t)dy * dst_fb->width + dx;
        for(int j = 0; j < h; j++){
            memcpy(dst8, src8, (size_t)w);
            src8 += src_fb->width;
            dst8 += dst_fb->width;
        }
        return;
    }
    if(src_fb->fmt == FBDRAW_FMT_ARGB8888 && dst_fb->fmt == FBDRAW_FMT_C8){
        // shadow 落盘: 栈上(cached)行缓冲逐像素反查, 再整行 memcpy 突发写显存
        const uint32_t* src = src_fb->vaddr + (size_t)sy * src_fb->width + sx;
        uint8_t* dst8 = (uint8_t*)dst_fb->vaddr + (size_t)dy * dst_fb->width + dx;
        uint8_t row[w];
        for(int j = 0; j < h; j++){
            for(int i = 0; i < w; i++)
                row[i] = c8pal_index(src[i]);
            memcpy(dst8, row, (size_t)w);
            src += src_fb->width;
            dst8 += dst_fb->width;
        }
        return;
    }
    if(src_fb->fmt == FBDRAW_FMT_C8 && dst_fb->fmt == FBDRAW_FMT_ARGB8888){
        // 索引素材展开进 shadow
        const uint8_t* src8 = (const uint8_t*)src_fb->vaddr + (size_t)sy * src_fb->width + sx;
        uint32_t* dst = dst_fb->vaddr + (size_t)dy * dst_fb->width + dx;
        for(int j = 0; j < h; j++){
            for(int i = 0; i < w; i++)
                dst[i] = c8pal_color(src8[i]);
            src8 += src_fb->width;
            dst += dst_fb->width;
        }
        return;
    }

    const uint32_t* src = src_fb->vaddr + sy * src_fb->width + sx;
    uint32_t* dst = dst_fb->vaddr + dy * dst_fb->width + dx;
    for(int j = 0; j < h; j++){
        memcpy(dst, src, (size_t)w * 4);
        src += src_fb->width;
        dst += dst_fb->width;
    }
}

int32_t fbdraw_text_width(const char* text, const lv_font_t* font, int32_t letter_space) {
    int32_t width = 0;
    uint32_t ofs = 0;
    uint32_t codepoint;
    while ((codepoint = lv_text_encoded_next(text, &ofs)) != 0) {
        uint32_t codepoint_next = lv_text_encoded_next(&text[ofs], NULL);
        int32_t w = (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next);
        if (w > 0) {
            width += w + letter_space;
        }
    }
    if (width > 0) width -= letter_space;
    return width;
}

void fbdraw_text(fbdraw_fb_t* fb, fbdraw_rect_t* rect, const char* text, const lv_font_t* font, uint32_t color,int32_t line_h,int32_t letter_space) {
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
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next) + letter_space;
            continue;
        }

        lv_draw_buf_t * glyph_draw_buf = lv_draw_buf_create_ex(lv_draw_buf_get_font_handlers(),
                                                               g_dsc.box_w, g_dsc.box_h,
                                                               LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
        if(!glyph_draw_buf) {
            cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next) + letter_space;
            continue;
        }

        g_dsc.req_raw_bitmap = 0;
        const lv_draw_buf_t * glyph_buf = (const lv_draw_buf_t *)lv_font_get_glyph_bitmap(&g_dsc, glyph_draw_buf);

        if(glyph_buf && glyph_buf->data && glyph_buf->header.cf == LV_COLOR_FORMAT_A8) {
            const uint8_t * a8 = (const uint8_t *)glyph_buf->data;
            const uint32_t stride = glyph_buf->header.stride;

            /* 参照 LVGL label 的基线计算：y 视为"行顶部" */
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
                    /* rect->y 是排版参考线不是裁剪线：font_registry 收紧过 line_height 的
                     * 字体，基线上方(line_height - base_line)容不下最高字形（hinting 后的
                     * 大写字母/汉字），墨迹会越出 rect 上缘 1~S(3) px，只按 fb 边界裁剪 */
                    if(px < rect->x || px >= rect->x + rect->w || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    fbdraw_blend_px(fb, px, py, src_r, src_g, src_b, pixel_alpha);
                }
            }
        }

        lv_font_glyph_release_draw_data(&g_dsc);
        lv_draw_buf_destroy(glyph_draw_buf);

        /* glyph advance（含 kerning + letter_space） */
        cursor_x += (int32_t)lv_font_get_glyph_width(font, codepoint, codepoint_next) + letter_space;
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
                    /* 与 fbdraw_text 相同：rect 上缘不裁剪（收紧行高字体的墨迹会越界） */
                    if(px < rect->x || px >= rect->x + rect->w || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    fbdraw_blend_px(fb, px, py, src_r, src_g, src_b, pixel_alpha);
                }
            }
        }

        lv_font_glyph_release_draw_data(&g_dsc);
        lv_draw_buf_destroy(glyph_draw_buf);

        cursor_y += line_height;
    }
}

void fbdraw_text_rot90(fbdraw_fb_t* fb, fbdraw_rect_t* rect,
                       const char* text, const lv_font_t* font, uint32_t color, int32_t letter_space) {
    // 1. 分配临时缓冲，宽高互换（与 fbdraw_barcode_rot90 相同模式）
    int buf_w = rect->h;
    int buf_h = rect->w;
    uint32_t* buf = calloc(buf_w * buf_h, sizeof(uint32_t));
    if (!buf) return;

    // 2. 在临时缓冲中水平渲染文字
    fbdraw_fb_t tmp_fb = { .vaddr = buf, .width = buf_w, .height = buf_h };
    fbdraw_rect_t tmp_rect = { .x = 0, .y = 0, .w = buf_w, .h = buf_h };
    fbdraw_text(&tmp_fb, &tmp_rect, text, font, color, 0, letter_space);

    // 3. 顺时针旋转 +90° (CW) 并写入目标
    for (int y = rect->y; y < rect->y + rect->h; y++) {
        for (int x = rect->x; x < rect->x + rect->w; x++) {
            int local_x = y - rect->y;
            int local_y = (rect->w - 1) - (x - rect->x);
            if (local_x < 0 || local_x >= buf_w || local_y < 0 || local_y >= buf_h) continue;
            uint32_t pixel = buf[local_y * buf_w + local_x];
            uint8_t pa = (pixel >> 24) & 0xFF;
            if (pa == 0) continue;
            fbdraw_blend_px(fb, x, y, (pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF, pa);
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
                    /* 与 fbdraw_text 相同：rect 上缘不裁剪（收紧行高字体的墨迹会越界） */
                    if(px < rect->x || px >= rect->x + rect->w || py >= rect->y + rect->h) continue;
                    if(px < 0 || px >= fb->width || py < 0 || py >= fb->height) continue;

                    fbdraw_blend_px(fb, px, py, src_r, src_g, src_b, pixel_alpha);
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
                uint32_t bgra_pixel = *((uint32_t *)(pixdata) + src_x + src_y * w);
                uint32_t rgb_pixel = (bgra_pixel & 0x000000FF) << 16 | (bgra_pixel & 0x0000FF00) | (bgra_pixel & 0x00FF0000) >> 16 | (bgra_pixel & 0xFF000000);
                fbdraw_write_px(fb, x, y, rgb_pixel);
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


    // cacheasset 与 overlay 显存同格式(C8 开关下是索引):同格式直贴,
    // 跨格式(如画进 8888 shadow)走 copy_rect 的分派
    fbdraw_fb_t src_fb = { .vaddr = (uint32_t*)pixdata, .width = w, .height = h,
                           .fmt = FBDRAW_OVERLAY_FMT };
    fbdraw_rect_t src_rect = { .x = 0, .y = 0, .w = w, .h = h };
    fbdraw_copy_rect(&src_fb, fb, &src_rect, rect);
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

            const uint32_t src_px = fbdraw_read_px(src_fb, src_x, src_y);
            const uint8_t sa = (src_px >> 24) & 0xFF;
            if(sa == 0) continue;

            uint8_t a = sa;
            if(opacity != 255) a = (uint8_t)(((uint32_t)sa * opacity + 127u) / 255u);
            if(a == 0) continue;

            const uint8_t r = (src_px >> 16) & 0xFF;
            const uint8_t g = (src_px >> 8) & 0xFF;
            const uint8_t b = src_px & 0xFF;

            fbdraw_blend_px(dst_fb, x, y, r, g, b, a);
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
    int barcode_height = buf_h - font_height - S(3);

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
    fbdst.fmt = FBDRAW_FMT_ARGB8888; // 临时缓冲恒 8888,只在最终落盘分派

    fbdraw_rect_t dst_rect;
    dst_rect.x = 0;
    dst_rect.y = barcode_height+S(3);
    dst_rect.w = buf_w;
    dst_rect.h = font_height;

    fbdraw_text(&fbdst, &dst_rect, str, font, 0xFF000000, 0, 0);

    for(int y = rect->y; y < rect->y + rect->h; y++){
        for(int x = rect->x; x < rect->x + rect->w; x++){
            // Rotate the barcode and text buffer -90 degrees (ccw) and blit it into destination framebuffer at rect
            // The buffer buf is [rect->w x rect->h], but is to be output -90deg rotated into fb->vaddr
            // (x', y') in destination corresponds to (rect->h - 1 - y, x) in the buf
            int local_x = (rect->h - 1) - (y - rect->y);
            int local_y = x - rect->x;
            if (local_x >= 0 && local_x < buf_w && local_y >= 0 && local_y < buf_h) {
                uint32_t pixel = buf[local_y * buf_w + local_x];
                fbdraw_write_px(fb, x, y, pixel);
            }
        }
    }

    free(buf);
    free(barcode_data);
}