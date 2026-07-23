
#include "overlay/opinfo.h"
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "utils/timer.h"
#include "utils/stb_image.h"
#include "utils/misc.h"
#include "config.h"
#include "ui_metrics.h"
#include "render/fbdraw.h"
#include "utils/cacheassets.h"
#include "utils/imgscale.h"
#include "ui/font_registry.h"
#include <src/misc/lv_text_private.h>
#include <src/misc/lv_math.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// ============================================================================
// 元素引擎
//
// 所有元素先画进堆上的"影子缓冲"（cached 内存，始终持有当前帧完整画面），
// 再按脏区一次 copy 落盘到显存。重叠的元素在初始化时被归入同一个"重叠组"：
// 组内任一成员动画推进时，整组先清区域、再按 z 序（元素列表顺序）重画。
// 这推广了旧实现里 fade 三角 + logo 手工合成的思路：
//   - fade 反复混合不会累积（每次从透明重画）
//   - 重叠区域每像素每帧只写一次显存，扫描线抓不到中间态
//   - 半透明混合的读操作全部发生在 cached 内存，uncached 显存零回读
// ============================================================================

typedef enum {
    ANIMATION_EINK_FIRST_BLACK,
    ANIMATION_EINK_FIRST_WHITE,
    ANIMATION_EINK_SECOND_BLACK,
    ANIMATION_EINK_SECOND_WHITE,
    ANIMATION_EINK_IDLE,
    ANIMATION_EINK_CONTENT
} opinfo_eink_state_t;

typedef struct {
    fbdraw_rect_t content; // 元素最终落位矩形（绘制与文本裁剪用）
    fbdraw_rect_t bbox;    // 分组/清除范围：move 元素为运动路径并集，其余等于 content
    int group;             // 重叠组代表元素的下标

    // 动画运行态
    int cpidx, cpcnt;             // typewriter/scramble：已稳定/总 codepoint 数
    opinfo_eink_state_t eink;     // eink 闪烁状态机
    int fade_value;               // fade：当前不透明度
    int wipe_value;               // wipe：当前已划入宽/高（物理，随 direction）
    int grow_value;               // grow：当前半径（360 基准）
    int scroll_value;             // scroll：当前偏移（物理）
    int cur_dx, cur_dy;           // move/sway：当前相对落点的偏移（物理）
    int sprite_idx;               // sprite：当前帧下标
    int anim_tick;                // sprite/sway：帧内计数器（不依赖全局帧号，免溢出）

    // image 元素的像素来源（用户图或 cacheasset，show 时解析）
    uint32_t* src_addr;
    int src_w, src_h;
    fbdraw_fmt_t src_fmt;  // cacheasset 与显存同格式；用户图恒 8888（量化后仍是展开像素）

    unsigned int visible : 1;  // 首次绘制后置位，组重画时参与；end_frame 退场后清零
    unsigned int dirty : 1;
    unsigned int done : 1;
    unsigned int blink_on : 1; // blink：当前处于显示相
} opinfo_el_state_t;

// 引擎控制块。timer 回调线程与 worker 线程都会引用它，而 prts_timer_cancel
// 不等待正在运行的回调结束（见 utils/timer.h），所以控制块必须常驻（static）；
// 真正占内存的影子缓冲与元素状态数组在堆上，由 worker 负责回收。
// active 标志用来拦截"清理之后迟到的调度"。
typedef struct {
    overlay_t* overlay;
    olopinfo_params_t* params;

    int curr_frame;
    int count;
    olopinfo_element_t* els; // = params->elements，归 operator entry 所有

    opinfo_el_state_t* st;   // 堆：worker 内回收
    uint32_t* shadow;        // 堆：worker 内回收
    fbdraw_fb_t shadow_fb;

    bool has_loop;           // 存在 scroll 元素 -> 永不自然结束
    bool active;
} opinfo_engine_t;

static opinfo_engine_t s_engine;

static const lv_font_t* el_font(const olopinfo_element_t* el){
    return font_get((font_role_t)el->font_role, el->font_size);
}

static bool rect_intersect(const fbdraw_rect_t* a, const fbdraw_rect_t* b){
    if(a->w <= 0 || a->h <= 0 || b->w <= 0 || b->h <= 0) return false;
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

static void rect_union(fbdraw_rect_t* u, const fbdraw_rect_t* r, bool* set){
    if(r->w <= 0 || r->h <= 0) return;
    if(!*set){
        *u = *r;
        *set = true;
        return;
    }
    int x2 = u->x + u->w > r->x + r->w ? u->x + u->w : r->x + r->w;
    int y2 = u->y + u->h > r->y + r->h ? u->y + u->h : r->y + r->h;
    if(r->x < u->x) u->x = r->x;
    if(r->y < u->y) u->y = r->y;
    u->w = x2 - u->x;
    u->h = y2 - u->y;
}

// a 与 window 的交集写回 a；无交集时 a 的 w/h 置 0
static void rect_clip(fbdraw_rect_t* a, const fbdraw_rect_t* window){
    int x1 = a->x > window->x ? a->x : window->x;
    int y1 = a->y > window->y ? a->y : window->y;
    int x2 = a->x + a->w < window->x + window->w ? a->x + a->w : window->x + window->w;
    int y2 = a->y + a->h < window->y + window->h ? a->y + a->h : window->y + window->h;
    a->x = x1;
    a->y = y1;
    a->w = x2 > x1 ? x2 - x1 : 0;
    a->h = y2 > y1 ? y2 - y1 : 0;
}

// 半透明矩形的 src-over 填充（fbdraw_fill_rect 是裸写，fade 中的 rect 需要混合；
// 只在影子缓冲(cached)上用，逐像素回读没有 uncached 的代价）
static void fill_rect_blend(fbdraw_fb_t* fb, fbdraw_rect_t* rect, uint32_t color){
    uint8_t a = (color >> 24) & 0xFF;
    if(a == 255){
        fbdraw_fill_rect(fb, rect, color);
        return;
    }
    if(a == 0) return;
    fbdraw_rect_t r = *rect;
    fbdraw_rect_t screen = { 0, 0, fb->width, fb->height };
    rect_clip(&r, &screen);
    const uint8_t cr = (color >> 16) & 0xFF, cg = (color >> 8) & 0xFF, cb = color & 0xFF;
    for(int y = r.y; y < r.y + r.h; y++){
        uint32_t* row = fb->vaddr + y * fb->width;
        for(int x = r.x; x < r.x + r.w; x++){
            fbdraw_blend_over_at(&row[x], cr, cg, cb, a);
        }
    }
}

// scramble 的随机字符（xorshift，固件里无时钟/可重复性顾虑）
static uint32_t scramble_rand(void){
    static uint32_t s = 0x9E3779B9u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// 颜色 alpha 乘 opacity/255（fade 用于 text/rect 时走颜色通道）
static uint32_t color_mul_alpha(uint32_t color, int opacity){
    uint32_t a = (color >> 24) & 0xFF;
    a = (a * (uint32_t)opacity + 127u) / 255u;
    return (color & 0x00FFFFFF) | (a << 24);
}

static int bezier_ease(int frame, int total, int target){
    // 与旧实现一致的 cubic-bezier(0.42, 0, 0.58, 1)，每帧现算，不再预计算表
    uint32_t t = lv_map(frame, 0, total, 0, LV_BEZIER_VAL_MAX);
    int32_t step = lv_cubic_bezier(t,
        LV_BEZIER_VAL_FLOAT(0.42), LV_BEZIER_VAL_FLOAT(0),
        LV_BEZIER_VAL_FLOAT(0.58), LV_BEZIER_VAL_FLOAT(1));
    return (int)(((int64_t)step * target) >> LV_BEZIER_VAL_SHIFT);
}

// ---------------------------------------------------------------------------
// 绘制：每个元素都能按"当前动画状态"从头重画（组重组的前提）
// ---------------------------------------------------------------------------

// radius 是 360 基准的逻辑单位；物理上每个逻辑像素画成 UI_SCALE×UI_SCALE 的块，
// 块内共享一个 alpha，保证 720 下渐变的物理尺寸和 360 一致。
static void el_draw_corner_fade(fbdraw_fb_t* fb, int radius, uint32_t color){
    if(radius < 2){
        return;
    }

    // alpha 只依赖逻辑坐标 x+y，先查表算好，省掉每像素一次除法
    uint8_t alpha_lut[radius];
    for(int s = 0; s <= radius - 2; s++){
        alpha_lut[s] = 255 - (s * 255 / radius);
    }

    const uint32_t rgb = color & 0x00FFFFFF;

    for(int py = 0; py < S(radius - 1); py++){
        int ly = py / UI_SCALE;
        int span = S(radius - 1 - ly);   // 本行覆盖的像素数，靠右边缘对齐
        int fby = fb->height - 1 - py;
        uint32_t* row = fb->vaddr + fby * fb->width + (fb->width - span);

        for(int i = 0; i < span; i++){
            int px = span - 1 - i;       // px: 距右边缘的距离
            row[i] = rgb | ((uint32_t)alpha_lut[px / UI_SCALE + ly] << 24);
        }
    }
}

static void el_draw_eink_flash(fbdraw_fb_t* fb, fbdraw_rect_t* r, opinfo_eink_state_t state){
    // FIRST_WHITE / SECOND_WHITE / IDLE 显示白块（IDLE 保持上一次的白），
    // FIRST_BLACK / SECOND_BLACK 显示黑块
    uint32_t c = (state == ANIMATION_EINK_FIRST_BLACK || state == ANIMATION_EINK_SECOND_BLACK)
                     ? 0xFF000000 : 0xFFFFFFFF;
    fbdraw_fill_rect(fb, r, c);
}

// 按方向把 win 收缩成 wipe 已划入的窗口
static void wipe_window(fbdraw_rect_t* win, int dir, int v){
    switch(dir){
    case OPINFO_WIPE_RTL:
        win->x += win->w - v;
        win->w = v;
        break;
    case OPINFO_WIPE_TTB:
        win->h = v;
        break;
    case OPINFO_WIPE_BTT:
        win->y += win->h - v;
        win->h = v;
        break;
    default: // LTR
        win->w = v;
        break;
    }
}

// scramble：前 cpidx 个 codepoint 原样保留，其余换成随机字符（\n 保留，多行结构不变）。
// 随机字符是单字节 ASCII，不会超过原文的字节长度。
static void scramble_build_text(const olopinfo_element_t* el, const opinfo_el_state_t* st,
                                char* out, size_t out_sz){
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%&*+-/";
    uint32_t ofs = 0;
    size_t o = 0;

    for(int cp = 0; cp < st->cpidx; cp++){
        uint32_t prev = ofs;
        if(lv_text_encoded_next(el->text, &ofs) == 0) break;
        size_t len = ofs - prev;
        if(o + len >= out_sz) break;
        memcpy(out + o, el->text + prev, len);
        o += len;
    }

    uint32_t codepoint;
    while((codepoint = lv_text_encoded_next(el->text, &ofs)) != 0){
        if(o + 1 >= out_sz) break;
        out[o++] = (codepoint == '\n') ? '\n'
                                       : charset[scramble_rand() % (sizeof(charset) - 1)];
    }
    out[o] = '\0';
}

static void el_draw_text_rot90(fbdraw_fb_t* fb, const olopinfo_element_t* el, const fbdraw_rect_t* pos){
    fbdraw_rect_t r = *pos;
    const lv_font_t* font = el_font(el);
    int32_t ls = S(el->letter_space);

    if(el->bold_split){
        // 空格前 faux bold，空格后常规；无空格则整体 faux bold
        fbdraw_rect_t base = { r.x, r.y, S(el->w), r.h };
        const char* space = strchr(el->text, ' ');
        if(space){
            char bold_part[sizeof(el->text)];
            int bold_len = (int)(space - el->text);
            memcpy(bold_part, el->text, bold_len);
            bold_part[bold_len] = '\0';
            const char* reg_part = space + 1;

            int32_t bold_px = fbdraw_text_width(bold_part, font, ls);
            int32_t space_gap = S(6);

            // Faux bold: 渲染两次，第二次 x+S(1) 偏移加粗笔画
            fbdraw_rect_t rb = { base.x, base.y, base.w, bold_px };
            fbdraw_text_rot90(fb, &rb, bold_part, font, el->color, ls);
            fbdraw_rect_t rb_fb = { base.x + S(1), base.y, base.w, bold_px };
            fbdraw_text_rot90(fb, &rb_fb, bold_part, font, el->color, ls);

            int32_t reg_y = base.y + bold_px + space_gap;
            int32_t reg_h = base.y + base.h - reg_y;
            if(reg_h > 0 && reg_part[0] != '\0'){
                fbdraw_rect_t r2 = { base.x, reg_y, base.w, reg_h };
                fbdraw_text_rot90(fb, &r2, reg_part, font, el->color, ls);
            }
        } else {
            fbdraw_text_rot90(fb, &base, el->text, font, el->color, ls);
            fbdraw_rect_t r_fb = { base.x + S(1), base.y, base.w, base.h };
            fbdraw_text_rot90(fb, &r_fb, el->text, font, el->color, ls);
        }
        return;
    }

    fbdraw_rect_t base = { r.x, r.y, S(el->w), r.h };
    fbdraw_text_rot90(fb, &base, el->text, font, el->color, ls);
    if(el->faux_bold){
        fbdraw_rect_t r_fb = { base.x + S(1), base.y, base.w, base.h };
        fbdraw_text_rot90(fb, &r_fb, el->text, font, el->color, ls);
    }
}

static void el_draw(opinfo_engine_t* d, int i){
    olopinfo_element_t* el = &d->els[i];
    opinfo_el_state_t* st = &d->st[i];
    fbdraw_fb_t* fb = &d->shadow_fb;

    if(el->anim == OPINFO_ANIM_BLINK && !st->blink_on){
        return; // 熄灭相：组重组已清区域，不画即消失
    }

    // move 进行中的当前位置（非 move 时偏移恒为 0）
    fbdraw_rect_t r = st->content;
    r.x += st->cur_dx;
    r.y += st->cur_dy;

    fbdraw_rect_t sr;
    fbdraw_fb_t src;

    switch(el->type){
    case OPINFO_EL_TEXT: {
        if(el->anim == OPINFO_ANIM_TYPEWRITER){
            fbdraw_text_range(fb, &r, el->text, el_font(el), el->color,
                              S(el->line_height), 0, st->cpidx + 1);
            break;
        }
        if(el->anim == OPINFO_ANIM_SCRAMBLE && !st->done){
            char scratch[sizeof(el->text)];
            scramble_build_text(el, st, scratch, sizeof(scratch));
            fbdraw_text(fb, &r, scratch, el_font(el), el->color,
                        S(el->line_height), S(el->letter_space));
            break;
        }
        uint32_t color = el->color;
        if(el->anim == OPINFO_ANIM_FADE){
            color = color_mul_alpha(color, st->fade_value);
        }
        fbdraw_text(fb, &r, el->text, el_font(el), color,
                    S(el->line_height), S(el->letter_space));
        break;
    }

    case OPINFO_EL_TEXT_ROT90:
        el_draw_text_rot90(fb, el, &r);
        break;

    case OPINFO_EL_IMAGE:
        if(el->anim == OPINFO_ANIM_EINK && st->eink != ANIMATION_EINK_CONTENT){
            el_draw_eink_flash(fb, &r, st->eink);
            break;
        }
        if(!st->src_addr){
            break;
        }
        src.vaddr = st->src_addr;
        src.width = st->src_w;
        src.height = st->src_h;
        src.fmt = st->src_fmt;
        sr.x = 0; sr.y = 0; sr.w = st->src_w; sr.h = st->src_h;

        if(el->anim == OPINFO_ANIM_FADE){
            fbdraw_alpha_opacity_rect(&src, fb, &sr, &r, (uint8_t)st->fade_value);
        }
        else if(el->anim == OPINFO_ANIM_WIPE){
            fbdraw_rect_t dr = r;
            wipe_window(&dr, el->wipe_dir, st->wipe_value);
            sr.x = dr.x - r.x;
            sr.y = dr.y - r.y;
            sr.w = dr.w;
            sr.h = dr.h;
            fbdraw_copy_rect(&src, fb, &sr, &dr);
        }
        else if(el->anim == OPINFO_ANIM_SCROLL){
            // 垂直循环滚动：src 从 offset 行起贴到顶部，回绕部分贴到底部
            int off = st->scroll_value;
            if(off < 0) off = 0;
            if(off > st->src_h) off = st->src_h;

            fbdraw_rect_t s1 = { 0, off, st->src_w, st->src_h - off };
            fbdraw_rect_t d1 = { r.x, r.y, st->src_w, st->src_h - off };
            fbdraw_copy_rect(&src, fb, &s1, &d1);

            fbdraw_rect_t s2 = { 0, 0, st->src_w, off };
            fbdraw_rect_t d2 = { r.x, r.y + (st->src_h - off), st->src_w, off };
            fbdraw_copy_rect(&src, fb, &s2, &d2);
        }
        else if(el->anim == OPINFO_ANIM_SPRITE){
            // 横向 strip：按当前帧下标裁一格贴出（bbox 已是单帧大小）
            int fw = el->frames > 1 ? st->src_w / el->frames : st->src_w;
            fbdraw_rect_t ss = { st->sprite_idx * fw, 0, fw, st->src_h };
            fbdraw_rect_t dr = { r.x, r.y, fw, st->src_h };
            fbdraw_copy_rect(&src, fb, &ss, &dr);
        }
        else {
            fbdraw_copy_rect(&src, fb, &sr, &r);
        }
        break;

    case OPINFO_EL_RECT: {
        if(el->anim == OPINFO_ANIM_EINK && st->eink != ANIMATION_EINK_CONTENT){
            el_draw_eink_flash(fb, &r, st->eink);
            break;
        }
        uint32_t color = el->color;
        if(el->anim == OPINFO_ANIM_FADE){
            color = color_mul_alpha(color, st->fade_value);
        }
        fbdraw_rect_t win = r;
        if(el->anim == OPINFO_ANIM_WIPE){
            wipe_window(&win, el->wipe_dir, st->wipe_value);
        }
        if(el->border_width > 0){
            // 空心边框：4 条边（左右不含角，避免 fade 时角上双重混合），与 wipe 窗口求交
            int bw = S(el->border_width);
            fbdraw_rect_t bars[4] = {
                { r.x, r.y, r.w, bw },                         // 上
                { r.x, r.y + r.h - bw, r.w, bw },              // 下
                { r.x, r.y + bw, bw, r.h - 2 * bw },           // 左
                { r.x + r.w - bw, r.y + bw, bw, r.h - 2 * bw } // 右
            };
            for(int b = 0; b < 4; b++){
                rect_clip(&bars[b], &win);
                if(bars[b].w > 0 && bars[b].h > 0){
                    fill_rect_blend(fb, &bars[b], color);
                }
            }
        } else {
            fill_rect_blend(fb, &win, color);
        }
        break;
    }

    case OPINFO_EL_BARCODE:
        if(el->anim == OPINFO_ANIM_EINK && st->eink != ANIMATION_EINK_CONTENT){
            el_draw_eink_flash(fb, &r, st->eink);
            break;
        }
        fbdraw_barcode_rot90(fb, &r, el->text, el_font(el));
        break;

    case OPINFO_EL_CORNER_FADE:
        el_draw_corner_fade(fb, st->grow_value, el->color);
        break;
    }
}

// ---------------------------------------------------------------------------
// 状态推进
// ---------------------------------------------------------------------------

static void el_advance(opinfo_engine_t* d, int i){
    olopinfo_element_t* el = &d->els[i];
    opinfo_el_state_t* st = &d->st[i];
    int frame = d->curr_frame;
    int speed = el->speed > 0 ? el->speed : 1;

    // 退场：到 end_frame 隐藏并计为播放完毕，组重组机制会把区域清掉
    if(el->end_frame > 0 && frame >= el->end_frame){
        if(st->visible || !st->done){
            st->visible = 0;
            st->dirty = 1;
            st->done = 1;
        }
        return;
    }

    if(frame < el->start_frame){
        return;
    }

    switch(el->anim){
    case OPINFO_ANIM_NONE:
        if(!st->done){
            st->dirty = 1;
            st->done = 1;
        }
        break;

    case OPINFO_ANIM_TYPEWRITER:
        if(st->done) break;
        if(st->cpidx >= st->cpcnt){
            st->done = 1;
            break;
        }
        // 与旧实现一致：按全局帧号取模，而不是相对 start_frame
        if(frame % speed == 0){
            st->cpidx++;
            st->dirty = 1;
            if(st->cpidx >= st->cpcnt) st->done = 1;
        }
        break;

    case OPINFO_ANIM_EINK:
        if(st->done) break;
        if(frame % speed == 0){
            st->eink++;
            st->dirty = 1;
            if(st->eink >= ANIMATION_EINK_CONTENT){
                st->eink = ANIMATION_EINK_CONTENT;
                st->done = 1;
            }
        }
        break;

    case OPINFO_ANIM_FADE:
        if(st->done) break;
        st->fade_value += speed;
        if(st->fade_value >= 255){
            st->fade_value = 255;
            st->done = 1;
        }
        st->dirty = 1;
        break;

    case OPINFO_ANIM_WIPE: {
        if(st->done) break;
        int f = frame - el->start_frame;
        int target = (el->wipe_dir == OPINFO_WIPE_TTB || el->wipe_dir == OPINFO_WIPE_BTT)
                         ? st->content.h : st->content.w;
        if(f >= speed){
            st->wipe_value = target;
            st->done = 1;
        } else {
            st->wipe_value = bezier_ease(f, speed, target);
        }
        st->dirty = 1;
        break;
    }

    case OPINFO_ANIM_MOVE: {
        if(st->done) break;
        int f = frame - el->start_frame;
        if(f >= speed){
            st->cur_dx = 0;
            st->cur_dy = 0;
            st->done = 1;
        } else {
            st->cur_dx = S(el->from_dx) - bezier_ease(f, speed, S(el->from_dx));
            st->cur_dy = S(el->from_dy) - bezier_ease(f, speed, S(el->from_dy));
        }
        st->dirty = 1;
        break;
    }

    case OPINFO_ANIM_SCRAMBLE:
        if(st->done) break;
        if(frame % speed == 0 && st->cpidx < st->cpcnt){
            st->cpidx++;
        }
        if(st->cpidx >= st->cpcnt){
            st->done = 1; // 本帧以真实文本收尾
        }
        st->dirty = 1; // 未稳定字符每帧跳变
        break;

    case OPINFO_ANIM_BLINK:
        if((frame - el->start_frame) % speed == 0){
            st->blink_on = !st->blink_on;
            st->dirty = 1;
        }
        break;

    case OPINFO_ANIM_SCROLL:
        st->scroll_value -= S(speed);
        if(st->scroll_value <= 0){
            st->scroll_value = st->src_h;
        }
        st->dirty = 1;
        break;

    case OPINFO_ANIM_GROW:
        if(st->done) break;
        st->grow_value += speed;
        if(st->grow_value >= el->w){
            st->grow_value = el->w;
            st->done = 1;
        }
        st->dirty = 1;
        break;

    case OPINFO_ANIM_SPRITE:
        if(el->frames <= 1) break;
        if(++st->anim_tick >= speed){
            st->anim_tick = 0;
            if(++st->sprite_idx >= el->frames) st->sprite_idx = 0;
            st->dirty = 1;
        }
        break;

    case OPINFO_ANIM_SWAY: {
        // anim_tick 走一整圈 0..speed 映射到 0..360°，from_dx/from_dy 为半摆幅。
        // sin 是简谐运动（端点慢、中间快），比线性往复自然。只在落点变化时标脏，
        // 端点附近相邻帧偏移相同就不触发整组重画，省 uncached 显存带宽。
        if(++st->anim_tick >= speed) st->anim_tick = 0;
        int deg = speed > 0 ? (int)((int64_t)st->anim_tick * 360 / speed) : 0;
        int32_t s = lv_trigo_sin((int16_t)deg); // -32767..32767
        int ndx = (int)(((int64_t)S(el->from_dx) * s) / 32767);
        int ndy = (int)(((int64_t)S(el->from_dy) * s) / 32767);
        if(ndx != st->cur_dx || ndy != st->cur_dy){
            st->cur_dx = ndx;
            st->cur_dy = ndy;
            st->dirty = 1;
        }
        break;
    }
    }

    if(st->dirty){
        st->visible = 1;
    }
}

// ---------------------------------------------------------------------------
// bbox 解析 / 重叠组
// ---------------------------------------------------------------------------

static void el_resolve(opinfo_engine_t* d, int i){
    olopinfo_element_t* el = &d->els[i];
    opinfo_el_state_t* st = &d->st[i];

    st->cpcnt = lv_text_get_encoded_length(el->text);
    st->eink = ANIMATION_EINK_FIRST_BLACK;

    int w_phys = 0, h_phys = 0;

    switch(el->type){
    case OPINFO_EL_IMAGE:
        if(el->cacheasset_id >= 0){
            int aw = 0, ah = 0;
            uint8_t* aa = NULL;
            cacheassets_get_asset_from_global((cacheasset_asset_id_t)el->cacheasset_id, &aw, &ah, &aa);
            st->src_addr = (uint32_t*)aa;
            st->src_w = aw;
            st->src_h = ah;
            st->src_fmt = FBDRAW_OVERLAY_FMT;
        } else {
            st->src_addr = el->image_addr;
            st->src_w = el->image_w;
            st->src_h = el->image_h;
            st->src_fmt = FBDRAW_FMT_ARGB8888;
        }
        if(!st->src_addr){
            st->src_w = 0;
            st->src_h = 0;
        }
        w_phys = st->src_w;
        h_phys = st->src_h;
        if(el->anim == OPINFO_ANIM_SPRITE && el->frames > 1){
            w_phys = st->src_w / el->frames; // 落位/分组按单帧，不是整张 sheet
        }
        break;

    case OPINFO_EL_TEXT: {
        const lv_font_t* font = el_font(el);
        int font_lh = (int)lv_font_get_line_height(font);
        int line_h = el->line_height > 0 ? S(el->line_height) : font_lh;
        int lines = 1;
        for(const char* p = el->text; *p; p++){
            if(*p == '\n') lines++;
        }
        w_phys = el->w > 0 ? S(el->w) : 0; // 缺省宽度在 anchor 解析后补到屏幕右缘
        h_phys = el->h > 0 ? S(el->h) : (lines - 1) * line_h + font_lh;
        break;
    }

    case OPINFO_EL_TEXT_ROT90:
        w_phys = S(el->w) + ((el->faux_bold || el->bold_split) ? S(1) : 0);
        h_phys = S(el->h);
        break;

    case OPINFO_EL_RECT:
    case OPINFO_EL_BARCODE:
        w_phys = S(el->w);
        h_phys = S(el->h);
        break;

    case OPINFO_EL_CORNER_FADE:
        // 覆盖到目标半径的右下角正方形
        w_phys = S(el->w - 1);
        h_phys = S(el->w - 1);
        break;
    }

    int x, y;
    if(el->type == OPINFO_EL_CORNER_FADE){
        x = OVERLAY_WIDTH - w_phys;
        y = OVERLAY_HEIGHT - h_phys;
    } else {
        x = (el->anchor == OPINFO_ANCHOR_TR || el->anchor == OPINFO_ANCHOR_BR)
                ? OVERLAY_WIDTH - S(el->x) - w_phys : S(el->x);
        y = (el->anchor == OPINFO_ANCHOR_BL || el->anchor == OPINFO_ANCHOR_BR)
                ? OVERLAY_HEIGHT - S(el->y) - h_phys : S(el->y);
    }

    if(el->type == OPINFO_EL_TEXT && el->w <= 0){
        w_phys = OVERLAY_WIDTH - x;
    }

    st->content.x = x;
    st->content.y = y;
    st->content.w = w_phys;
    st->content.h = h_phys;

    // 分组/清除范围：move 元素取运动路径（起点∪终点）的并集，其余等于落位矩形
    st->bbox = st->content;
    if(el->anim == OPINFO_ANIM_MOVE){
        fbdraw_rect_t start_rect = st->content;
        start_rect.x += S(el->from_dx);
        start_rect.y += S(el->from_dy);
        bool set = true;
        rect_union(&st->bbox, &start_rect, &set);
        st->cur_dx = S(el->from_dx);
        st->cur_dy = S(el->from_dy);
    } else if(el->anim == OPINFO_ANIM_SWAY){
        // 摆动范围是落点 ±摆幅，两端都并进清除范围，否则拖影
        fbdraw_rect_t a = st->content, b = st->content;
        a.x += S(el->from_dx); a.y += S(el->from_dy);
        b.x -= S(el->from_dx); b.y -= S(el->from_dy);
        bool set = true;
        rect_union(&st->bbox, &a, &set);
        rect_union(&st->bbox, &b, &set);
        // cur_dx/dy 初始 0：相位从 0 起，sin(0)=0，起手在落点
    }

    // 收紧行高的字体(见 font_registry 的 lh_pct)基线上方容不下最高字形，
    // fbdraw 文本不再裁 rect 上缘，墨迹最多越出 content 上缘 S(3) px。
    // 清除/落盘范围随之上扩，否则动画重画会留拖影、增量落盘会漏拷顶部几行。
    if(el->type == OPINFO_EL_TEXT){
        st->bbox.y -= S(3);
        st->bbox.h += S(3);
    }
}

static int group_find(opinfo_engine_t* d, int i){
    while(d->st[i].group != i){
        d->st[i].group = d->st[d->st[i].group].group;
        i = d->st[i].group;
    }
    return i;
}

static void build_groups(opinfo_engine_t* d){
    for(int i = 0; i < d->count; i++){
        d->st[i].group = i;
    }
    for(int i = 0; i < d->count; i++){
        for(int j = i + 1; j < d->count; j++){
            if(!rect_intersect(&d->st[i].bbox, &d->st[j].bbox)) continue;
            int ri = group_find(d, i);
            int rj = group_find(d, j);
            if(ri == rj) continue;
            // 代表取小下标，保证组代表在成员之前被遍历到
            if(ri < rj) d->st[rj].group = ri;
            else        d->st[ri].group = rj;
        }
    }
    for(int i = 0; i < d->count; i++){
        d->st[i].group = group_find(d, i);
    }
}

// ---------------------------------------------------------------------------
// 每帧合成：推进状态 -> 组重组/增量绘制（影子缓冲）-> 脏区落盘
// 返回 true 表示全部元素播放完毕（无循环元素时）
// ---------------------------------------------------------------------------

static bool engine_compose(opinfo_engine_t* d){
    fbdraw_fb_t vram_fb;
    vram_fb.vaddr = (uint32_t*)d->overlay->overlay_buf.vaddr;
    vram_fb.width = OVERLAY_WIDTH;
    vram_fb.height = OVERLAY_HEIGHT;
    vram_fb.fmt = FBDRAW_OVERLAY_FMT; // shadow 恒 8888,落盘 copy_rect 自动量化

    for(int i = 0; i < d->count; i++){
        el_advance(d, i);
    }

    for(int g = 0; g < d->count; g++){
        if(d->st[g].group != g) continue; // 只从组代表进入

        bool dirty = false;
        int members = 0;
        for(int i = 0; i < d->count; i++){
            if(d->st[i].group != g) continue;
            members++;
            dirty |= d->st[i].dirty;
        }
        if(!dirty) continue;

        // 独占的打字机文本：字形只增不减，直接增量画新 codepoint，
        // 免去整段文本的逐帧重排。（end_frame 退场时 visible=0，落入下面的
        // 完整重组路径清区域）
        if(members == 1 && d->els[g].anim == OPINFO_ANIM_TYPEWRITER && d->st[g].visible){
            opinfo_el_state_t* st = &d->st[g];
            fbdraw_text_range(&d->shadow_fb, &st->content, d->els[g].text,
                              el_font(&d->els[g]), d->els[g].color,
                              S(d->els[g].line_height), st->cpidx, st->cpidx + 1);
            // 按 bbox 落盘：字形墨迹可越出 content 上缘（见 el_resolve 的 bbox 上扩）
            fbdraw_copy_rect(&d->shadow_fb, &vram_fb, &st->bbox, &st->bbox);
            st->dirty = 0;
            continue;
        }

        // 组重组：清成员区域 -> 按 z 序重画 -> 并集一次落盘
        fbdraw_rect_t u = {0, 0, 0, 0};
        bool u_set = false;
        for(int i = 0; i < d->count; i++){
            if(d->st[i].group != g) continue;
            fbdraw_fill_rect(&d->shadow_fb, &d->st[i].bbox, 0x00000000);
            rect_union(&u, &d->st[i].bbox, &u_set);
        }
        for(int i = 0; i < d->count; i++){
            if(d->st[i].group != g) continue;
            if(d->st[i].visible){
                el_draw(d, i);
            }
            d->st[i].dirty = 0;
        }
        if(u_set){
            fbdraw_copy_rect(&d->shadow_fb, &vram_fb, &u, &u);
        }
    }

    d->curr_frame++;

    if(d->has_loop){
        return false;
    }
    for(int i = 0; i < d->count; i++){
        if(!d->st[i].done) return false;
    }
    return true;
}

// 清理只在 worker 内做（timer 回调与 worker 是两个线程，别处 free 会 UAF），
// 且必须把 overlay_timer_handle 归零，否则 overlay_abort() 会一直等。
static void engine_cleanup(opinfo_engine_t* d){
    overlay_t* overlay = d->overlay;
    d->active = false;
    prts_timer_cancel(overlay->overlay_timer_handle);
    free(d->st);
    d->st = NULL;
    free(d->shadow);
    d->shadow = NULL;
    d->shadow_fb.vaddr = NULL;
    overlay->overlay_timer_handle = 0;
}

static void engine_worker(void* userdata, int skipped_frames){
    (void)skipped_frames; // 与旧 arknights worker 一致：不处理跳帧
    opinfo_engine_t* d = (opinfo_engine_t*)userdata;

    if(!d->active){
        // 清理之后迟到的调度（timer cancel 不等在途回调），资源已释放，直接忽略
        return;
    }

    if(d->overlay->request_abort){
        log_debug("opinfo engine worker: request abort");
        engine_cleanup(d);
        return;
    }

    if(engine_compose(d)){
        log_debug("opinfo engine worker: all elements finished");
        engine_cleanup(d);
    }
}

// 定时器回调。来自普瑞塞斯的 rt 启动的 sigev_thread 线程，只做 schedule。
static void engine_timer_cb(void* userdata, bool is_last){
    (void)is_last;
    opinfo_engine_t* d = (opinfo_engine_t*)userdata;
    overlay_worker_schedule(d->overlay, engine_worker, d);
}

void overlay_opinfo_show_elements(overlay_t* overlay, olopinfo_params_t* params){
    log_info("overlay_opinfo_show_elements: type=%d element_count=%d",
             params->type, params->element_count);

    if(params->element_count <= 0 || params->elements == NULL){
        log_error("overlay_opinfo_show_elements: 元素列表为空，跳过显示");
        return;
    }

    opinfo_engine_t* d = &s_engine;
    if(d->active){
        // 正常流程 overlay_abort 已等 worker 清理完；防御式拦一下
        log_error("overlay_opinfo_show_elements: 上一个 opinfo 尚未清理，跳过显示");
        return;
    }

    memset(d, 0, sizeof(*d));
    d->overlay = overlay;
    d->params = params;
    d->count = params->element_count;
    d->els = params->elements;

    d->st = calloc(d->count, sizeof(opinfo_el_state_t));
    d->shadow = calloc((size_t)OVERLAY_WIDTH * OVERLAY_HEIGHT, 4);
    if(!d->st || !d->shadow){
        log_error("overlay_opinfo_show_elements: 内存分配失败");
        free(d->st);
        d->st = NULL;
        free(d->shadow);
        d->shadow = NULL;
        return;
    }
    d->shadow_fb.vaddr = d->shadow;
    d->shadow_fb.width = OVERLAY_WIDTH;
    d->shadow_fb.height = OVERLAY_HEIGHT;
    d->shadow_fb.fmt = FBDRAW_FMT_ARGB8888;

    for(int i = 0; i < d->count; i++){
        el_resolve(d, i);
        // 循环动画让引擎永不自然结束；带 end_frame 的循环元素会退场，不算
        opinfo_anim_t a = d->els[i].anim;
        if((a == OPINFO_ANIM_SCROLL || a == OPINFO_ANIM_BLINK ||
            a == OPINFO_ANIM_SPRITE || a == OPINFO_ANIM_SWAY)
           && d->els[i].end_frame == 0){
            d->has_loop = true;
        }
    }
    build_groups(d);

    // 先把图层挪到屏外再直绘单 buffer，绘制过程不可见
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

#if OVERLAY_USE_C8
    // 层已离屏:恢复烘焙段(上一个 owner 可能整表覆盖过) + 写本次颜色池 + 上传。
    // image 模式独占 1..254,其余从动态段基址写起
    c8pal_restore_baked();
    c8pal_write_range(params->type == OPINFO_TYPE_IMAGE ? C8PAL_IMAGE_BASE : C8PAL_DYN_BASE,
                      params->c8_pool, params->c8_pool_n);
    c8pal_commit();
#endif

    memset(overlay->overlay_buf.vaddr, 0, OVERLAY_BUF_BYTES);

    overlay->request_abort = 0;
    overlay->overlay_used = 1;
    d->active = true;

    // 第 0 帧（静态模板部分）同步画好再入场，与旧 init_template 语义一致
    engine_compose(d);

    prts_timer_create(
        &overlay->overlay_timer_handle,
        OVERLAY_ANIMATION_STEP_TIME,
        OVERLAY_ANIMATION_STEP_TIME,
        -1,
        engine_timer_cb,
        d
    );

    int duration = params->duration > 0 ? params->duration : 1000 * 1000;
    layer_animation_ease_in_out_move(
        overlay->layer_animation,
        DRM_WARPPER_LAYER_OVERLAY,
        0, OVERLAY_HEIGHT,
        0, 0,
        duration, 0
    );
}

// ============================================================================
// 元素列表构建（image）
// ============================================================================

void overlay_opinfo_element_init(olopinfo_element_t* el){
    memset(el, 0, sizeof(*el));
    el->cacheasset_id = -1;
    el->color = 0xFFFFFFFF;
    el->font_role = FONT_BODY;
    el->font_size = 14;
}

int overlay_opinfo_build_image_elements(olopinfo_params_t* params){
    olopinfo_element_t* el = malloc(sizeof(*el));
    if(!el){
        log_error("build_image_elements: malloc failed");
        params->elements = NULL;
        params->element_count = 0;
        return -1;
    }
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->anim = OPINFO_ANIM_NONE;
    safe_strcpy(el->image_path, sizeof(el->image_path), params->image_path);

    params->elements = el;
    params->element_count = 1;
    return 0;
}

// ============================================================================
// arknights 专用实现（手搓，不走元素引擎）
// ============================================================================

// 标记这个元素有没有更新过
typedef struct{
    unsigned int operator_name : 1 ;
    unsigned int operator_code : 1 ;
    unsigned int barcode : 1 ;
    unsigned int staff_text : 1 ;
    unsigned int class_icon : 1 ;
    unsigned int aux_text : 1 ;
    unsigned int fade_color : 1;
    unsigned int rhodes:1;
    unsigned int arrow:1;
    unsigned int logo_fade:1;
    unsigned int ak_bar_swipe:1;
    unsigned int div_line_upper:1;
    unsigned int div_line_lower:1;


} arknights_overlay_update_t;

typedef struct {
    overlay_t* overlay;
    olopinfo_params_t* params;

    int curr_frame;

    // codepoint index,codepoint count
    int operator_name_cpidx;
    int operator_name_cpcnt;

    int operator_code_cpidx;
    int operator_code_cpcnt;

    int stuff_text_cpidx;
    int stuff_text_cpcnt;

    int aux_text_cpidx;
    int aux_text_cpcnt;

    int color_fade_value;

    int logo_fade_value;

    int ak_bar_swipe_bezeir_values[OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT];

    // 复用引擎的 eink 状态枚举（成员定义一致）
    opinfo_eink_state_t class_icon_state;
    opinfo_eink_state_t barcode_state;

    int div_line_bezeir_values[OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT];

    int arrow_y_value;

    arknights_overlay_update_t update;
} arknights_overlay_worker_data_t;


// radius 是 360 基准的逻辑单位；物理上每个逻辑像素画成 UI_SCALE×UI_SCALE 的块，
// 块内共享一个 alpha，保证 720 下渐变的物理尺寸和 360 一致。
//
// fade 三角和 logo 在右下角是同一片区域。单 buffer 直绘时如果“先画 fade 再贴 logo”
// 分两次落盘，扫描线会抓到没有 logo 的中间态闪一下。所以每行先在栈上(cached)把
// fade + logo 合成出最终像素，再一次 memcpy 进显存：每个像素每帧只写一次最终值。
// 三角以外的 logo 像素跳过不画（logo 淡入头几帧三角还没长满时不透明度极低，无感）。
static void draw_color_fade_and_logo(uint32_t* vaddr,int radius,uint32_t color,
                                     uint32_t* logo_addr,int logo_w,int logo_h,int logo_opacity){
    if(radius < 2){
        return;
    }

    // alpha 只依赖逻辑坐标 x+y，先查表算好，省掉每像素一次除法
    uint8_t alpha_lut[radius];
    for(int s = 0; s <= radius - 2; s++){
        alpha_lut[s] = 255 - (s * 255 / radius);
    }

    const int logo_x0 = OVERLAY_WIDTH - logo_w - S(10);
    const int logo_y0 = OVERLAY_HEIGHT - logo_h - S(10);
    const uint32_t rgb = color & 0x00FFFFFF;

#if OVERLAY_USE_C8
    // fade 渐变的专用量化:通用 LUT 的 alpha 只有 16 桶,32 级 theme ramp 会被
    // 折半出台阶。这里按 ramp 公式(与 c8pal_pool_add_ramp 一致)精确定位每级的
    // 表项,量化时在相邻两级间做 Bayer 抖动。ramp 项是 load 时入池的精确色,
    // find_exact 必中;万一没中(池被挤爆)退回通用反查
    uint8_t ramp_idx[C8PAL_THEME_RAMP_LEVELS + 1];
    for(int i = 0; i < C8PAL_THEME_RAMP_LEVELS; i++){
        uint32_t a = 255u * (uint32_t)(C8PAL_THEME_RAMP_LEVELS - i) / C8PAL_THEME_RAMP_LEVELS;
        uint32_t c = (a << 24) | rgb;
        int idx = c8pal_find_exact(c);
        ramp_idx[i] = idx >= 0 ? (uint8_t)idx : c8pal_index(c);
    }
    ramp_idx[C8PAL_THEME_RAMP_LEVELS] = C8PAL_IDX_TRANSPARENT;
#endif

    uint32_t row[S(radius - 1)];

    for(int py = 0; py < S(radius - 1); py++){
        int ly = py / UI_SCALE;
        int span = S(radius - 1 - ly);   // 本行 fade 覆盖的像素数，靠右边缘对齐
        int fby = OVERLAY_HEIGHT - 1 - py;
        int row_x0 = OVERLAY_WIDTH - span;

        for(int i = 0; i < span; i++){
            int px = span - 1 - i;       // px: 距右边缘的距离
            row[i] = rgb | ((uint32_t)alpha_lut[px / UI_SCALE + ly] << 24);
        }

        // logo 覆盖的列范围(混合结果不再是纯 ramp 色,量化走通用反查)
        int logo_l = span, logo_r = -1;
        if(logo_addr && logo_opacity > 0){
            int lrow = fby - logo_y0;
            if(lrow >= 0 && lrow < logo_h){
                for(int lcol = 0; lcol < logo_w; lcol++){
                    int i = logo_x0 + lcol - row_x0;
                    if(i < 0 || i >= span) continue;
                    if(i < logo_l) logo_l = i;
                    if(i > logo_r) logo_r = i;
                    uint32_t px32 = logo_addr[lrow * logo_w + lcol];
                    uint8_t a = (px32 >> 24) & 0xFF;
                    if(logo_opacity != 255) a = (uint8_t)(((uint32_t)a * logo_opacity + 127u) / 255u);
                    fbdraw_blend_over_at(&row[i], (px32 >> 16) & 0xFF, (px32 >> 8) & 0xFF, px32 & 0xFF, a);
                }
            }
        }

#if OVERLAY_USE_C8
        // 行内合成仍是 8888(fade+logo 的 8bit alpha 语义保留),落盘时量化。
        // 纯 fade 像素按 alpha 直接定位 ramp 层级,层间 Bayer 抖动(64 阶,
        // 断层化成 stipple 密度渐变);logo 混过的像素走通用反查
        uint8_t crow[span];
        for(int i = 0; i < span; i++){
            if(i >= logo_l && i <= logo_r){
                crow[i] = c8pal_index(row[i]);
                continue;
            }
            uint32_t a = row[i] >> 24;
            int num = (int)(255u - a) * C8PAL_THEME_RAMP_LEVELS;
            int li = num / 255;
            int rem = num % 255;
            if((int)fbdraw_bayer8[fby & 7][(row_x0 + i) & 7] * 4 < rem) li++;
            if(li > C8PAL_THEME_RAMP_LEVELS) li = C8PAL_THEME_RAMP_LEVELS;
            crow[i] = ramp_idx[li];
        }
        memcpy((uint8_t*)vaddr + (size_t)fby * OVERLAY_WIDTH + row_x0, crow, (size_t)span);
#else
        memcpy(vaddr + fby * OVERLAY_WIDTH + row_x0, row, (size_t)span * 4);
#endif
    }
}
// 绘制arknights overlay的worker.
// 不处理跳帧了。
static void arknights_overlay_worker(void *userdata,int skipped_frames){
    arknights_overlay_worker_data_t* data = (arknights_overlay_worker_data_t*)userdata;

    // 是否要求我们退出
    if(data->overlay->request_abort){
        prts_timer_cancel(data->overlay->overlay_timer_handle);
        data->overlay->overlay_timer_handle = 0;
        log_debug("arknights overlay worker: request abort");
        return;
    }

    // =============  状态转移  ==================
    // == 文本 打字机效果 START

    // name
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_NAME_START_FRAME && data->operator_name_cpidx != data->operator_name_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_NAME_FRAME_PER_CODEPOINT == 0){
            data->operator_name_cpidx++;
            data->update.operator_name = 1;
        }
    }

    //code
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_CODE_START_FRAME && data->operator_code_cpidx != data->operator_code_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_CODE_FRAME_PER_CODEPOINT == 0){
            data->operator_code_cpidx++;
            data->update.operator_code = 1;
        }
    }

    //stuff text
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_START_FRAME && data->stuff_text_cpidx != data->stuff_text_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_FRAME_PER_CODEPOINT == 0){
            data->stuff_text_cpidx++;
            data->update.staff_text = 1;
        }
    }

    //aux text
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_AUX_TEXT_START_FRAME && data->aux_text_cpidx != data->aux_text_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_AUX_TEXT_FRAME_PER_CODEPOINT == 0){
            data->aux_text_cpidx++;
            data->update.aux_text = 1;
        }
    }

    // == 文本 打字机效果 END

    // == BARCODE 和 CLASSICON 的 Eink效果
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_CLASSICON_START_FRAME && data->class_icon_state != ANIMATION_EINK_CONTENT){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_CLASSICON_FRAME_PER_STATE == 0){
            data->class_icon_state++;
            data->update.class_icon = 1;
        }
    }

    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_BARCODE_START_FRAME && data->barcode_state != ANIMATION_EINK_CONTENT){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_BARCODE_FRAME_PER_STATE == 0){
            data->barcode_state++;
            data->update.barcode = 1;
        }
    }
    // == BARCODE 和 CLASSICON 的 Eink效果 end ==

    // == color fade 和 logo fade start ==
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_COLOR_FADE_START_FRAME && data->color_fade_value < OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE){
        data->color_fade_value += OVERLAY_ANIMATION_OPINFO_COLOR_FADE_VALUE_PER_FRAME;
        if(data->color_fade_value >= OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE){
            data->color_fade_value = OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE;
        }
        data->update.fade_color = 1;
    }
    // == color fade 和 logo fade end ==

    // == logo swipe start
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_LOGO_FADE_START_FRAME && data->logo_fade_value < 255){
        data->logo_fade_value += OVERLAY_ANIMATION_OPINFO_LOGO_FADE_VALUE_PER_FRAME;
        if(data->logo_fade_value >= 255){
            data->logo_fade_value = 255;
        }
        data->update.logo_fade = 1;
        // logo 和 colorfade是冲突的
        // colorfade 也需要重画
        data->update.fade_color = 1;
    }
    // == logo swipe end ==

    // == ak bar swipe start

    int ak_bar_swipe_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_START_FRAME;
    if (ak_bar_swipe_frame >= 0 && ak_bar_swipe_frame < OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT){
        data->update.ak_bar_swipe = 1;
    }

    // division line start

    int div_line_upper_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_LINE_UPPER_START_FRAME;
    if (div_line_upper_frame >= 0 && div_line_upper_frame < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT){
        data->update.div_line_upper = 1;
    }

    int div_line_lower_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_LINE_LOWER_START_FRAME;
    if (div_line_lower_frame >= 0 && div_line_lower_frame < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT){
        data->update.div_line_lower = 1;
    }

    // =========== 绘制 ================

    int asset_w, asset_h;
    uint8_t* asset_addr;
    arknights_overlay_update_t * update = &data->update;

    uint32_t* vaddr = (uint32_t*)data->overlay->overlay_buf.vaddr;

    fbdraw_fb_t fbdst;
    fbdraw_fb_t fbsrc;
    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;
    fbdst.fmt = FBDRAW_OVERLAY_FMT;


    fbdraw_rect_t dst_rect;
    fbdraw_rect_t src_rect;

    olopinfo_params_t* params = data->params;

    // == 文本 start ==
    // name
    if(update->operator_name){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect,
            params->operator_name,
            font_get(FONT_DISPLAY, 40),
            0xFFFFFFFF,0,
            data->operator_name_cpidx, data->operator_name_cpidx + 1
        );
        update->operator_name = 0;
    }
    // code
    if(update->operator_code){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect,
            params->operator_code,
            font_get(FONT_BODY, 14),
            0xFFFFFFFF,0,
            data->operator_code_cpidx, data->operator_code_cpidx + 1
        );
        update->operator_code = 0;
    }
    //stuff text
    if(update->staff_text){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect,
            params->staff_text,
            font_get(FONT_BODY, 14),
            0xFFFFFFFF,0,
            data->stuff_text_cpidx, data->stuff_text_cpidx + 1
        );
        update->staff_text = 0;
    }
    //aux text
    if(update->aux_text){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect,
            params->aux_text,
            font_get(FONT_BODY, 14),
            0xFFFFFFFF,S(14),
            data->aux_text_cpidx, data->aux_text_cpidx + 1
        );
        update->aux_text = 0;
    }
    // == 文本 end ==

    // == BARCODE 和 CLASSICON 的 Eink效果 start ==
    if(update->barcode){
        dst_rect.x = S(1);
        dst_rect.y = OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y;
        dst_rect.w = OVERLAY_ARKNIGHTS_BARCODE_WIDTH;
        dst_rect.h = OVERLAY_ARKNIGHTS_BARCODE_HEIGHT;

        if (data->barcode_state == ANIMATION_EINK_FIRST_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->barcode_state == ANIMATION_EINK_FIRST_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->barcode_state == ANIMATION_EINK_SECOND_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->barcode_state == ANIMATION_EINK_SECOND_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->barcode_state == ANIMATION_EINK_IDLE){
            //does nothing
        }
        else{
            fbdraw_barcode_rot90(&fbdst, &dst_rect, params->barcode_text, font_get(FONT_BODY, 14));
        }

        update->barcode = 0;
    }

    if(update->class_icon){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y;
        dst_rect.w = params->class_w;
        dst_rect.h = params->class_h;

        if (data->class_icon_state == ANIMATION_EINK_FIRST_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->class_icon_state == ANIMATION_EINK_FIRST_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->class_icon_state == ANIMATION_EINK_SECOND_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->class_icon_state == ANIMATION_EINK_SECOND_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->class_icon_state == ANIMATION_EINK_IDLE){
            //does nothing
        }
        else{
            fbsrc.vaddr = (uint32_t*) params->class_addr;
            fbsrc.width = params->class_w;
            fbsrc.height = params->class_h;
            fbsrc.fmt = FBDRAW_FMT_ARGB8888;

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.w = params->class_w;
            src_rect.h = params->class_h;

            fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
        }

        update->class_icon = 0;
    }
    // == BARCODE 和 CLASSICON 的 Eink效果 end ==

    // == color fade + logo：右下角同一片区域，合成后单次落盘，避免竞争闪烁
    if(update->fade_color || update->logo_fade){
        draw_color_fade_and_logo(
            vaddr, data->color_fade_value, params->color,
            params->logo_addr, params->logo_w, params->logo_h,
            data->logo_fade_value
        );
        update->fade_color = 0;
        update->logo_fade = 0;
    }

    if (update->ak_bar_swipe){
        cacheassets_get_asset_from_global(CACHE_ASSETS_AK_BAR, &asset_w, &asset_h, &asset_addr);
        fbsrc.vaddr = (uint32_t*)asset_addr;
        fbsrc.width = asset_w;
        fbsrc.height = asset_h;
        fbsrc.fmt = FBDRAW_OVERLAY_FMT;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = data->ak_bar_swipe_bezeir_values[ak_bar_swipe_frame];
        src_rect.h = asset_h;

        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y;
        dst_rect.w = data->ak_bar_swipe_bezeir_values[ak_bar_swipe_frame];
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

        update->ak_bar_swipe = 0;
    }

    if(update->div_line_upper){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y;
        dst_rect.w = data->div_line_bezeir_values[div_line_upper_frame];
        dst_rect.h = S(1);
        fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

        update->div_line_upper = 0;
    }

    if(update->div_line_lower){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y;
        dst_rect.w = data->div_line_bezeir_values[div_line_lower_frame];
        dst_rect.h = S(1);
        fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

        update->div_line_lower = 0;
    }

    // ARROWS. it always redraw

    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_ARROW, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;
    fbsrc.fmt = FBDRAW_OVERLAY_FMT;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = data->arrow_y_value;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y + (asset_h - data->arrow_y_value);
    dst_rect.w = asset_w;
    dst_rect.h = data->arrow_y_value;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    src_rect.x = 0;
    src_rect.y = data->arrow_y_value;
    src_rect.w = asset_w;
    src_rect.h = asset_h - data->arrow_y_value;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h - data->arrow_y_value;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    data->arrow_y_value -= OVERLAY_ANIMATION_OPINFO_ARROW_Y_INCR_PER_FRAME;
    if(data->arrow_y_value <= 0){
        data->arrow_y_value = asset_h;
    }

    data->curr_frame++;
}

// 定时器回调。来自普瑞塞斯 的 rt 启动的 sigev_thread 线程。
static void arknights_overlay_worker_timer_cb(void *userdata,bool is_last){
    arknights_overlay_worker_data_t* data = (arknights_overlay_worker_data_t*)userdata;
    overlay_worker_schedule(data->overlay,arknights_overlay_worker,data);
}

static void init_template_arknights_overlay(uint32_t* vaddr, olopinfo_params_t* params){
    log_info("init_template_arknights: rhodes_text=[%s]", params->rhodes_text);

    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    memset(vaddr, 0, OVERLAY_BUF_BYTES);


    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;
    fbdst.fmt = FBDRAW_OVERLAY_FMT;

    // 本函数的 fbsrc 全部来自 cacheasset(与显存同格式)
    fbsrc.fmt = FBDRAW_OVERLAY_FMT;

    int asset_w,asset_h;
    uint8_t* asset_addr;

    // TOP_LEFT_RECT
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RECT, &asset_w, &asset_h, &asset_addr);

    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_ARKNIGHTS_RECT_OFFSET_X;
    dst_rect.y = 0;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // BTM_LEFT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_BTM_LEFT_BAR, &asset_w, &asset_h, &asset_addr);

    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = 0;
    dst_rect.y = OVERLAY_HEIGHT - asset_h;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_RIGHT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_BAR, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = 0;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_RIGHT_BAR 自定义文字（空格前 faux bold，空格后常规）
    if (params->top_right_bar_text[0] != '\0') {
        // 用黑色覆盖图片内嵌文字（图片内基准坐标 42,314 ~ 52,416，随分辨率 S()）
        int bar_screen_x = OVERLAY_WIDTH - asset_w;
        dst_rect.x = bar_screen_x + S(42);
        dst_rect.y = S(314);
        dst_rect.w = S(10);
        dst_rect.h = S(102);
        fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);

        const char *space = strchr(params->top_right_bar_text, ' ');
        if (space) {
            char bold_part[40];
            int bold_len = space - params->top_right_bar_text;
            memcpy(bold_part, params->top_right_bar_text, bold_len);
            bold_part[bold_len] = '\0';
            const char *reg_part = space + 1;

            int32_t bold_px = fbdraw_text_width(bold_part, font_get(FONT_DISPLAY, 10), S(2));
            int32_t space_gap = S(6);

            // Faux bold: 渲染两次，第二次 x+S(1) 偏移加粗笔画
            fbdraw_rect_t r = { dst_rect.x, dst_rect.y, S(10), bold_px };
            fbdraw_text_rot90(&fbdst, &r, bold_part, font_get(FONT_DISPLAY, 10), 0xFFFFFFFF, S(2));
            fbdraw_rect_t r_fb = { dst_rect.x + S(1), dst_rect.y, S(10), bold_px };
            fbdraw_text_rot90(&fbdst, &r_fb, bold_part, font_get(FONT_DISPLAY, 10), 0xFFFFFFFF, S(2));

            // Regular: 渲染一次（无 faux bold）
            int32_t reg_y = dst_rect.y + bold_px + space_gap;
            int32_t reg_h = dst_rect.y + dst_rect.h - reg_y;
            if (reg_h > 0 && reg_part[0] != '\0') {
                fbdraw_rect_t r2 = { dst_rect.x, reg_y, S(10), reg_h };
                fbdraw_text_rot90(&fbdst, &r2, reg_part, font_get(FONT_DISPLAY, 10), 0xFFFFFFFF, S(2));
            }
        } else {
            // 无空格，全部 faux bold
            fbdraw_text_rot90(&fbdst, &dst_rect, params->top_right_bar_text,
                              font_get(FONT_DISPLAY, 10), 0xFFFFFFFF, S(2));
            fbdraw_rect_t r_fb = { dst_rect.x + S(1), dst_rect.y, dst_rect.w, dst_rect.h };
            fbdraw_text_rot90(&fbdst, &r_fb, params->top_right_bar_text,
                              font_get(FONT_DISPLAY, 10), 0xFFFFFFFF, S(2));
        }
    }

    // TOP_LEFT_RHODES
    if (params->rhodes_text[0] != '\0') {
        // 用户自定义文字替代 logo（顺时针旋转 +90° 显示，72px Bold）
        dst_rect.x = 0;
        dst_rect.y = S(5);
        dst_rect.w = S(67);
        dst_rect.h = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y - S(5);
        fbdraw_text_rot90(&fbdst, &dst_rect, params->rhodes_text, font_get(FONT_DISPLAY, 72), 0xFFFFFFFF, 0);
    } else {
        // 默认缓存图
        cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RHODES, &asset_w, &asset_h, &asset_addr);
        fbsrc.vaddr = (uint32_t*)asset_addr;
        fbsrc.width = asset_w;
        fbsrc.height = asset_h;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = asset_w;
        src_rect.h = asset_h;

        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.w = asset_w;
        dst_rect.h = asset_h;

        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }

}

void overlay_opinfo_show_arknights(overlay_t* overlay,olopinfo_params_t* params){
    log_info("overlay_opinfo_show_arknights");

    // 先把图层挪到屏外再直绘单 buffer，绘制过程不可见
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

#if OVERLAY_USE_C8
    // 层已离屏:恢复烘焙段 + 写本次颜色池(theme ramp/class/logo) + 上传
    c8pal_restore_baked();
    c8pal_write_range(C8PAL_DYN_BASE, params->c8_pool, params->c8_pool_n);
    c8pal_commit();
#endif

    init_template_arknights_overlay((uint32_t*)overlay->overlay_buf.vaddr, params);

    static arknights_overlay_worker_data_t data;
    memset(&data, 0, sizeof(arknights_overlay_worker_data_t));
    data.overlay = overlay;
    data.params = params;
    data.operator_name_cpcnt = lv_text_get_encoded_length(params->operator_name);
    data.operator_code_cpcnt = lv_text_get_encoded_length(params->operator_code);
    data.stuff_text_cpcnt = lv_text_get_encoded_length(params->staff_text);
    data.aux_text_cpcnt = lv_text_get_encoded_length(params->aux_text);

    int h,w;
    uint8_t* addr;
    cacheassets_get_asset_from_global(CACHE_ASSETS_AK_BAR, &w, &h, &addr);

    int32_t ctlx1 = LV_BEZIER_VAL_FLOAT(0.42);
    int32_t ctly1 = LV_BEZIER_VAL_FLOAT(0);
    int32_t ctx2 = LV_BEZIER_VAL_FLOAT(0.58);
    int32_t cty2 = LV_BEZIER_VAL_FLOAT(1);

    for(int i = 0; i < OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT; i++){
        uint32_t t = lv_map(i, 0, OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT, 0, LV_BEZIER_VAL_MAX);
        int32_t step = lv_cubic_bezier(t, ctlx1, ctly1, ctx2, cty2);
        int32_t new_value;
        new_value = step * w;
        new_value = new_value >> LV_BEZIER_VAL_SHIFT;
        data.ak_bar_swipe_bezeir_values[i] = new_value;
    }

    for(int i = 0; i < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT; i++){
        uint32_t t = lv_map(i, 0, OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT, 0, LV_BEZIER_VAL_MAX);
        int32_t step = lv_cubic_bezier(t, ctlx1, ctly1, ctx2, cty2);
        int32_t new_value;
        new_value = step * OVERLAY_ARKNIGHTS_LINE_WIDTH;
        new_value = new_value >> LV_BEZIER_VAL_SHIFT;
        data.div_line_bezeir_values[i] = new_value;
    }

    overlay->request_abort = 0;
    overlay->overlay_used = 1;

    prts_timer_create(
        &overlay->overlay_timer_handle  ,
        0,
        OVERLAY_ANIMATION_STEP_TIME,
        -1,
        arknights_overlay_worker_timer_cb,
        &data
    );


    layer_animation_ease_in_out_move(
        overlay->layer_animation,
        DRM_WARPPER_LAYER_OVERLAY,
        0, OVERLAY_HEIGHT,
        0, 0,
        1 * 1000 * 1000, 0
    );
}

// ============================================================================
// 用户图片加载/释放
// ============================================================================

#if OVERLAY_USE_C8
// 用户图量化进颜色池(就地改写像素为量化展开,绘制路径不感知)。
// 只在 load 阶段调用,绝不上传——旧 overlay 可能还在播退场动画,
// 上传统一推迟到 show 入口(层已离屏)。
static void pool_quantize_image(olopinfo_params_t* params, int pool_cap,
                                const char* path, uint32_t* px, int w, int h, int quota)
{
    if(!px) return;
    int remain = pool_cap - params->c8_pool_n;
    if(quota > remain) quota = remain;
    if(quota < 2){
        log_warn("opinfo c8 pool exhausted, %s falls back to LUT nearest", path);
        return;
    }
    uint32_t tmp[254];
    int n = c8pal_load_or_quantize(path, px, w, h, quota, tmp);
    if(n > 0)
        c8pal_pool_add(params->c8_pool, &params->c8_pool_n, pool_cap, tmp, n);
}
#endif

void overlay_opinfo_load_image(olopinfo_params_t* params){
    params->c8_pool_n = 0;
    if(params->type == OPINFO_TYPE_ARKNIGHTS){
        load_img_assets(params->class_path, &params->class_addr, &params->class_w, &params->class_h);
        load_img_assets(params->logo_path, &params->logo_addr, &params->logo_w, &params->logo_h);
        // class/logo 也是用户图，旧素材同样按基准放大
        if(params->class_addr){
            imgscale_rescale_nn_rgba(&params->class_addr, &params->class_w, &params->class_h, params->src_upscale, params->src_downscale);
        }
        if(params->logo_addr){
            imgscale_rescale_nn_rgba(&params->logo_addr, &params->logo_w, &params->logo_h, params->src_upscale, params->src_downscale);
        }
#if OVERLAY_USE_C8
        // theme ramp 先入池(corner fade 的半透明落点),再量化两张用户图
        c8pal_pool_add_ramp(params->c8_pool, &params->c8_pool_n, C8PAL_DYN_QUOTA,
                            params->color | 0xFF000000, C8PAL_THEME_RAMP_LEVELS);
        pool_quantize_image(params, C8PAL_DYN_QUOTA, params->class_path,
                            params->class_addr, params->class_w, params->class_h,
                            C8PAL_QUOTA_CLASS);
        pool_quantize_image(params, C8PAL_DYN_QUOTA, params->logo_path,
                            params->logo_addr, params->logo_w, params->logo_h,
                            C8PAL_QUOTA_AKLOGO);
#endif
        log_debug("loaded class: %s, w: %d, h: %d", params->class_path, params->class_w, params->class_h);
        log_debug("loaded logo: %s, w: %d, h: %d", params->logo_path, params->logo_w, params->logo_h);
        return;
    }

#if OVERLAY_USE_C8
    const int pool_cap = (params->type == OPINFO_TYPE_IMAGE) ? C8PAL_IMAGE_QUOTA
                                                             : C8PAL_DYN_QUOTA;
    // custom: 元素色先入池(数量小,保色相优先)。前几个唯一色带 alpha ramp
    // (文字 AA/FADE 的半透明落点),其余只写 opaque;黑/白烘焙段已有,跳过
    if(params->type == OPINFO_TYPE_CUSTOM){
        int ramps = 0;
        for(int i = 0; i < params->element_count; i++){
            uint32_t c = params->elements[i].color | 0xFF000000;
            if(c == 0xFFFFFFFF || c == 0xFF000000) continue;
            int known = 0;
            for(int j = 0; j < params->c8_pool_n; j++){
                if(params->c8_pool[j] == c){ known = 1; break; }
            }
            if(known) continue;
            if(ramps < C8PAL_COLOR_RAMPS_MAX){
                c8pal_pool_add_ramp(params->c8_pool, &params->c8_pool_n, pool_cap,
                                    c, C8PAL_COLOR_RAMP_LEVELS);
                ramps++;
            } else {
                c8pal_pool_add(params->c8_pool, &params->c8_pool_n, pool_cap, &c, 1);
            }
        }
    }
#endif

    for(int i = 0; i < params->element_count; i++){
        olopinfo_element_t* el = &params->elements[i];
        if(el->type != OPINFO_EL_IMAGE || el->cacheasset_id >= 0){
            continue;
        }
        load_img_assets(el->image_path, &el->image_addr, &el->image_w, &el->image_h);
        if(el->image_addr){
            imgscale_rescale_nn_rgba(&el->image_addr, &el->image_w, &el->image_h, params->src_upscale, params->src_downscale);
        }
#if OVERLAY_USE_C8
        pool_quantize_image(params, pool_cap, el->image_path,
                            el->image_addr, el->image_w, el->image_h,
                            (params->type == OPINFO_TYPE_IMAGE) ? C8PAL_IMAGE_QUOTA
                                                                : C8PAL_QUOTA_CUSTOM_IMG);
#endif
        log_debug("loaded opinfo image: %s, w: %d, h: %d", el->image_path, el->image_w, el->image_h);
    }
}

void overlay_opinfo_free_image(olopinfo_params_t* params){
    if(params->type == OPINFO_TYPE_ARKNIGHTS){
        if(params->class_addr){
            free(params->class_addr);
            params->class_addr = NULL;
            log_debug("freed class: %s", params->class_path);
        }
        if(params->logo_addr){
            free(params->logo_addr);
            params->logo_addr = NULL;
            log_debug("freed logo: %s", params->logo_path);
        }
        return;
    }
    for(int i = 0; i < params->element_count; i++){
        olopinfo_element_t* el = &params->elements[i];
        if(el->image_addr){
            free(el->image_addr);
            el->image_addr = NULL;
            log_debug("freed opinfo image: %s", el->image_path);
        }
    }
}

void overlay_opinfo_free_elements(olopinfo_params_t* params){
    overlay_opinfo_free_image(params);
    free(params->elements);
    params->elements = NULL;
    params->element_count = 0;
}
