
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
    fbdraw_rect_t bbox;   // 解析后的物理像素包围盒（也是绘制的裁剪区）
    int group;            // 重叠组代表元素的下标

    // 动画运行态
    int cpidx, cpcnt;             // typewriter：已显示/总 codepoint 数
    opinfo_eink_state_t eink;     // eink 闪烁状态机
    int fade_value;               // fade：当前不透明度
    int wipe_value;               // wipe：当前已划入宽度（物理）
    int grow_value;               // grow：当前半径（360 基准）
    int scroll_value;             // scroll：当前偏移（物理）

    // image 元素的像素来源（用户图或 cacheasset，show 时解析）
    uint32_t* src_addr;
    int src_w, src_h;

    unsigned int visible : 1; // 首次绘制后置位，组重画时参与
    unsigned int dirty : 1;
    unsigned int done : 1;
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

static void el_draw_text_rot90(fbdraw_fb_t* fb, const olopinfo_element_t* el, const opinfo_el_state_t* st){
    fbdraw_rect_t r = st->bbox;
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
    fbdraw_rect_t r = st->bbox;
    fbdraw_rect_t sr;
    fbdraw_fb_t src;

    switch(el->type){
    case OPINFO_EL_TEXT:
        if(el->anim == OPINFO_ANIM_TYPEWRITER){
            fbdraw_text_range(fb, &r, el->text, el_font(el), el->color,
                              S(el->line_height), 0, st->cpidx + 1);
        } else {
            fbdraw_text(fb, &r, el->text, el_font(el), el->color,
                        S(el->line_height), S(el->letter_space));
        }
        break;

    case OPINFO_EL_TEXT_ROT90:
        el_draw_text_rot90(fb, el, st);
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
        sr.x = 0; sr.y = 0; sr.w = st->src_w; sr.h = st->src_h;

        if(el->anim == OPINFO_ANIM_FADE){
            fbdraw_alpha_opacity_rect(&src, fb, &sr, &r, (uint8_t)st->fade_value);
        }
        else if(el->anim == OPINFO_ANIM_WIPE){
            sr.w = st->wipe_value;
            fbdraw_rect_t dr = r;
            dr.w = st->wipe_value;
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
        else {
            fbdraw_copy_rect(&src, fb, &sr, &r);
        }
        break;

    case OPINFO_EL_RECT:
        if(el->anim == OPINFO_ANIM_EINK && st->eink != ANIMATION_EINK_CONTENT){
            el_draw_eink_flash(fb, &r, st->eink);
            break;
        }
        if(el->anim == OPINFO_ANIM_WIPE){
            r.w = st->wipe_value;
        }
        fbdraw_fill_rect(fb, &r, el->color);
        break;

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
        if(f >= speed){
            st->wipe_value = st->bbox.w;
            st->done = 1;
        } else {
            st->wipe_value = bezier_ease(f, speed, st->bbox.w);
        }
        st->dirty = 1;
        break;
    }

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
        } else {
            st->src_addr = el->image_addr;
            st->src_w = el->image_w;
            st->src_h = el->image_h;
        }
        if(!st->src_addr){
            st->src_w = 0;
            st->src_h = 0;
        }
        w_phys = st->src_w;
        h_phys = st->src_h;
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

    st->bbox.x = x;
    st->bbox.y = y;
    st->bbox.w = w_phys;
    st->bbox.h = h_phys;
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
        // 免去整段文本的逐帧重排
        if(members == 1 && d->els[g].anim == OPINFO_ANIM_TYPEWRITER){
            opinfo_el_state_t* st = &d->st[g];
            fbdraw_text_range(&d->shadow_fb, &st->bbox, d->els[g].text,
                              el_font(&d->els[g]), d->els[g].color,
                              S(d->els[g].line_height), st->cpidx, st->cpidx + 1);
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

    for(int i = 0; i < d->count; i++){
        el_resolve(d, i);
        if(d->els[i].anim == OPINFO_ANIM_SCROLL){
            d->has_loop = true;
        }
    }
    build_groups(d);

    // 先把图层挪到屏外再直绘单 buffer，绘制过程不可见
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);
    memset(overlay->overlay_buf.vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);

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
// 元素列表构建（image / arknights 预设）
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

// cacheasset 的 360 基准宽度（asset 按当前分辨率制作，除回 UI_SCALE）
static int cacheasset_base_w(cacheasset_asset_id_t id){
    int w = 0, h = 0;
    uint8_t* addr = NULL;
    cacheassets_get_asset_from_global(id, &w, &h, &addr);
    return addr ? w / UI_SCALE : 0;
}

// arknights 通行证模板 = 元素引擎上的一个预设。
// 坐标/帧数/字体逐项对应旧的专用实现（宏为物理像素，除回 UI_SCALE 得 360 基准）。
int overlay_opinfo_build_arknights_elements(olopinfo_params_t* params){
    olopinfo_element_t tmp[OPINFO_ELEMENTS_MAX];
    int n = 0;
    olopinfo_element_t* el;

    const int btm_info_x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X / UI_SCALE;

    // ---- 静态模板（z 序最底）----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->cacheasset_id = CACHE_ASSETS_TOP_LEFT_RECT;
    el->x = OVERLAY_ARKNIGHTS_RECT_OFFSET_X / UI_SCALE;
    el->y = 0;

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->cacheasset_id = CACHE_ASSETS_BTM_LEFT_BAR;
    el->anchor = OPINFO_ANCHOR_BL;

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->cacheasset_id = CACHE_ASSETS_TOP_RIGHT_BAR;
    el->anchor = OPINFO_ANCHOR_TR;

    // TOP_RIGHT_BAR 自定义文字：先黑块盖掉图片内嵌文字，再画旋转文字
    if(params->top_right_bar_text[0] != '\0'){
        int bar_w = cacheasset_base_w(CACHE_ASSETS_TOP_RIGHT_BAR);
        if(bar_w > 0){
            int bar_x = OVERLAY_WIDTH / UI_SCALE - bar_w;

            el = &tmp[n++];
            overlay_opinfo_element_init(el);
            el->type = OPINFO_EL_RECT;
            el->x = bar_x + 42;
            el->y = 314;
            el->w = 10;
            el->h = 102;
            el->color = 0xFF000000;

            el = &tmp[n++];
            overlay_opinfo_element_init(el);
            el->type = OPINFO_EL_TEXT_ROT90;
            el->x = bar_x + 42;
            el->y = 314;
            el->w = 10;
            el->h = 102;
            el->font_role = FONT_DISPLAY;
            el->font_size = 10;
            el->letter_space = 2;
            el->bold_split = true;
            safe_strcpy(el->text, sizeof(el->text), params->top_right_bar_text);
        }
    }

    // TOP_LEFT_RHODES：自定义文字（旋转 90°, 72px）或默认缓存图
    if(params->rhodes_text[0] != '\0'){
        el = &tmp[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_TEXT_ROT90;
        el->x = 0;
        el->y = 5;
        el->w = 67;
        el->h = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y / UI_SCALE - 5;
        el->font_role = FONT_DISPLAY;
        el->font_size = 72;
        safe_strcpy(el->text, sizeof(el->text), params->rhodes_text);
    } else {
        el = &tmp[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_IMAGE;
        el->cacheasset_id = CACHE_ASSETS_TOP_LEFT_RHODES;
    }

    // ---- 右下角渐变三角 + logo（同组，由重组机制合成）----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_CORNER_FADE;
    el->anim = OPINFO_ANIM_GROW;
    el->w = OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_COLOR_FADE_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_COLOR_FADE_VALUE_PER_FRAME;
    el->color = params->color;

    if(params->logo_path[0] != '\0'){
        el = &tmp[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_IMAGE;
        el->anim = OPINFO_ANIM_FADE;
        el->anchor = OPINFO_ANCHOR_BR;
        el->x = 10;
        el->y = 10;
        el->start_frame = OVERLAY_ANIMATION_OPINFO_LOGO_FADE_START_FRAME;
        el->speed = OVERLAY_ANIMATION_OPINFO_LOGO_FADE_VALUE_PER_FRAME;
        safe_strcpy(el->image_path, sizeof(el->image_path), params->logo_path);
    }

    // ---- 打字机文本 ----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y / UI_SCALE;
    el->font_role = FONT_DISPLAY;
    el->font_size = 40;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_NAME_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_NAME_FRAME_PER_CODEPOINT;
    safe_strcpy(el->text, sizeof(el->text), params->operator_name);

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y / UI_SCALE;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_CODE_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_CODE_FRAME_PER_CODEPOINT;
    safe_strcpy(el->text, sizeof(el->text), params->operator_code);

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y / UI_SCALE;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_FRAME_PER_CODEPOINT;
    safe_strcpy(el->text, sizeof(el->text), params->staff_text);

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y / UI_SCALE;
    el->line_height = 14;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_AUX_TEXT_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_AUX_TEXT_FRAME_PER_CODEPOINT;
    safe_strcpy(el->text, sizeof(el->text), params->aux_text);

    // ---- 条形码 / 职业图标（Eink 闪烁）----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_BARCODE;
    el->anim = OPINFO_ANIM_EINK;
    el->x = 1;
    el->y = OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y / UI_SCALE;
    el->w = OVERLAY_ARKNIGHTS_BARCODE_WIDTH / UI_SCALE;
    el->h = OVERLAY_ARKNIGHTS_BARCODE_HEIGHT / UI_SCALE;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_BARCODE_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_BARCODE_FRAME_PER_STATE;
    safe_strcpy(el->text, sizeof(el->text), params->barcode_text);

    if(params->class_path[0] != '\0'){
        el = &tmp[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_IMAGE;
        el->anim = OPINFO_ANIM_EINK;
        el->x = btm_info_x;
        el->y = OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y / UI_SCALE;
        el->start_frame = OVERLAY_ANIMATION_OPINFO_CLASSICON_START_FRAME;
        el->speed = OVERLAY_ANIMATION_OPINFO_CLASSICON_FRAME_PER_STATE;
        safe_strcpy(el->image_path, sizeof(el->image_path), params->class_path);
    }

    // ---- AK bar / 分割线（贝塞尔划入）----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->cacheasset_id = CACHE_ASSETS_AK_BAR;
    el->anim = OPINFO_ANIM_WIPE;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y / UI_SCALE;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT;

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_RECT;
    el->anim = OPINFO_ANIM_WIPE;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y / UI_SCALE;
    el->w = OVERLAY_ARKNIGHTS_LINE_WIDTH / UI_SCALE;
    el->h = 1;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_LINE_UPPER_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT;

    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_RECT;
    el->anim = OPINFO_ANIM_WIPE;
    el->x = btm_info_x;
    el->y = OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y / UI_SCALE;
    el->w = OVERLAY_ARKNIGHTS_LINE_WIDTH / UI_SCALE;
    el->h = 1;
    el->start_frame = OVERLAY_ANIMATION_OPINFO_LINE_LOWER_START_FRAME;
    el->speed = OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT;

    // ---- 右上角循环滚动箭头（z 序最顶）----
    el = &tmp[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_IMAGE;
    el->cacheasset_id = CACHE_ASSETS_TOP_RIGHT_ARROW;
    el->anim = OPINFO_ANIM_SCROLL;
    el->anchor = OPINFO_ANCHOR_TR;
    el->y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y / UI_SCALE;
    el->speed = OVERLAY_ANIMATION_OPINFO_ARROW_Y_INCR_PER_FRAME;

    params->elements = malloc((size_t)n * sizeof(olopinfo_element_t));
    if(!params->elements){
        log_error("build_arknights_elements: malloc failed");
        params->element_count = 0;
        return -1;
    }
    memcpy(params->elements, tmp, (size_t)n * sizeof(olopinfo_element_t));
    params->element_count = n;
    return 0;
}

// ============================================================================
// 用户图片加载/释放
// ============================================================================

void overlay_opinfo_load_image(olopinfo_params_t* params){
    for(int i = 0; i < params->element_count; i++){
        olopinfo_element_t* el = &params->elements[i];
        if(el->type != OPINFO_EL_IMAGE || el->cacheasset_id >= 0){
            continue;
        }
        load_img_assets(el->image_path, &el->image_addr, &el->image_w, &el->image_h);
        if(el->image_addr){
            imgscale_upscale_nn_rgba(&el->image_addr, &el->image_w, &el->image_h, params->src_upscale);
        }
        log_debug("loaded opinfo image: %s, w: %d, h: %d", el->image_path, el->image_w, el->image_h);
    }
}

void overlay_opinfo_free_image(olopinfo_params_t* params){
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
