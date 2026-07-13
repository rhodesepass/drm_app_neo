#include "overlay_sim_presets.h"
#include "ui/font_registry.h"
#include "utils/misc.h"
#include "utils/respath.h"

#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 1. arknights 默认模板：覆盖 corner_fade+logo 合成、class eink、全套打字机
// ---------------------------------------------------------------------------
static int build_arknights_default(olopinfo_params_t* p){
    p->type = OPINFO_TYPE_ARKNIGHTS;
    safe_strcpy(p->operator_name, sizeof(p->operator_name), "AMIYA");
    safe_strcpy(p->operator_code, sizeof(p->operator_code), "ARKNIGHT - UNK0");
    safe_strcpy(p->barcode_text, sizeof(p->barcode_text), "OPERATOR - ARKNIGHTS");
    safe_strcpy(p->staff_text, sizeof(p->staff_text), "STAFF");
    safe_strcpy(p->aux_text, sizeof(p->aux_text),
                "Operator of Rhodes Island\nUndefined/Rhodes Island\n Hypergryph");
    p->color = 0xFF8B0000u;
    safe_strcpy(p->class_path, sizeof(p->class_path), respath("defaulticon.png"));
    safe_strcpy(p->logo_path, sizeof(p->logo_path), respath("prts_64_inv.png"));
    return overlay_opinfo_build_arknights_elements(p);
}

// ---------------------------------------------------------------------------
// 2. arknights 自定义文字：rhodes_text / top_right_bar_text(bold_split) / 换色
// ---------------------------------------------------------------------------
static int build_arknights_custom(olopinfo_params_t* p){
    p->type = OPINFO_TYPE_ARKNIGHTS;
    safe_strcpy(p->operator_name, sizeof(p->operator_name), "TEXAS");
    safe_strcpy(p->operator_code, sizeof(p->operator_code), "PENGUIN - LGD3");
    safe_strcpy(p->barcode_text, sizeof(p->barcode_text), "PENGUIN - LOGISTICS");
    safe_strcpy(p->staff_text, sizeof(p->staff_text), "COURIER");
    safe_strcpy(p->aux_text, sizeof(p->aux_text),
                "Courier of Penguin Logistics\nLungmen/Penguin Logistics\n Hypergryph");
    p->color = 0xFF14365Du;
    safe_strcpy(p->rhodes_text, sizeof(p->rhodes_text), "PENGUIN");
    safe_strcpy(p->top_right_bar_text, sizeof(p->top_right_bar_text), "EPASS SIMULATOR");
    return overlay_opinfo_build_arknights_elements(p);
}

// ---------------------------------------------------------------------------
// 3. custom 文本特效：typewriter / scramble / blink / corner_fade / barcode eink /
//    end_frame 临时字幕
// ---------------------------------------------------------------------------
static int build_custom_text_fx(olopinfo_params_t* p){
    p->type = OPINFO_TYPE_CUSTOM;
    olopinfo_element_t* els = calloc(8, sizeof(*els));
    if(!els) return -1;
    int n = 0;
    olopinfo_element_t* el;

    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_CORNER_FADE;
    el->anim = OPINFO_ANIM_GROW;
    el->w = 192;
    el->speed = 10;
    el->start_frame = 10;
    el->color = 0xFF14365Du;

    // 临时字幕：0 帧打字机进场，120 帧整体消失（end_frame 退场）
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = 40; el->y = 60;
    el->font_role = FONT_BODY; el->font_size = 14;
    el->start_frame = 0; el->speed = 2; el->end_frame = 120;
    safe_strcpy(el->text, sizeof(el->text), "ESTABLISHING LINK...");

    // 大标题打字机
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = 70; el->y = 380;
    el->font_role = FONT_DISPLAY; el->font_size = 40;
    el->start_frame = 20; el->speed = 3;
    safe_strcpy(el->text, sizeof(el->text), "SIMULATOR");

    // 乱码解码 ID
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_SCRAMBLE;
    el->x = 70; el->y = 430;
    el->font_role = FONT_DISPLAY; el->font_size = 14;
    el->start_frame = 30; el->speed = 2;
    safe_strcpy(el->text, sizeof(el->text), "ID // 042-C7#F9-EPASS");

    // 多行正文打字机
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_TYPEWRITER;
    el->x = 70; el->y = 460;
    el->line_height = 14;
    el->start_frame = 50; el->speed = 2;
    safe_strcpy(el->text, sizeof(el->text),
                "Element engine smoke test\nAll animations on one page\n PC simulator");

    // 闪烁红点
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_RECT;
    el->anim = OPINFO_ANIM_BLINK;
    el->x = 330; el->y = 20; el->w = 8; el->h = 8;
    el->color = 0xFFFF3B30u;
    el->start_frame = 30; el->speed = 10;

    // 条形码 eink
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_BARCODE;
    el->anim = OPINFO_ANIM_EINK;
    el->x = 1; el->y = 450; el->w = 50; el->h = 180;
    el->start_frame = 30; el->speed = 15;
    safe_strcpy(el->text, sizeof(el->text), "EPASS - SIM");

    p->elements = els;
    p->element_count = n;
    return 0;
}

// ---------------------------------------------------------------------------
// 4. custom 运动与几何：move 四方向（含边框矩形）/ wipe 四方向 / fade 文本与矩形 /
//    anchor 四角标记
// ---------------------------------------------------------------------------
static int build_custom_motion(olopinfo_params_t* p){
    p->type = OPINFO_TYPE_CUSTOM;
    olopinfo_element_t* els = calloc(16, sizeof(*els));
    if(!els) return -1;
    int n = 0;
    olopinfo_element_t* el;

    // anchor 四角标记（tl 之外的三个角验证锚定解析）
    static const opinfo_anchor_t anchors[4] = {
        OPINFO_ANCHOR_TL, OPINFO_ANCHOR_TR, OPINFO_ANCHOR_BL, OPINFO_ANCHOR_BR
    };
    for(int i = 0; i < 4; i++){
        el = &els[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_RECT;
        el->anchor = anchors[i];
        el->x = 4; el->y = 4; el->w = 12; el->h = 12;
        el->color = 0xFF34C759u;
    }

    // move 四方向进场的边框矩形（错峰）
    static const struct { int x, y, dx, dy, start; } moves[4] = {
        {  60, 120, -120,    0, 10 },
        { 200, 120,  120,    0, 20 },
        {  60, 200,    0, -150, 30 },
        { 200, 200,    0,  150, 40 },
    };
    for(int i = 0; i < 4; i++){
        el = &els[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_RECT;
        el->anim = OPINFO_ANIM_MOVE;
        el->x = moves[i].x; el->y = moves[i].y;
        el->w = 100; el->h = 60;
        el->border_width = 2;
        el->from_dx = moves[i].dx; el->from_dy = moves[i].dy;
        el->start_frame = moves[i].start; el->speed = 20;
    }

    // wipe 四方向：两条横条 + 两条竖条
    static const struct { int x, y, w, h, dir, start; } wipes[4] = {
        {  40, 300, 280,  6, OPINFO_WIPE_LTR, 60 },
        {  40, 320, 280,  6, OPINFO_WIPE_RTL, 70 },
        {  40, 340,   6, 120, OPINFO_WIPE_TTB, 80 },
        { 314, 340,   6, 120, OPINFO_WIPE_BTT, 90 },
    };
    for(int i = 0; i < 4; i++){
        el = &els[n++];
        overlay_opinfo_element_init(el);
        el->type = OPINFO_EL_RECT;
        el->anim = OPINFO_ANIM_WIPE;
        el->x = wipes[i].x; el->y = wipes[i].y;
        el->w = wipes[i].w; el->h = wipes[i].h;
        el->wipe_dir = wipes[i].dir;
        el->start_frame = wipes[i].start; el->speed = 40;
    }

    // fade 文本
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_TEXT;
    el->anim = OPINFO_ANIM_FADE;
    el->x = 70; el->y = 500;
    el->font_role = FONT_DISPLAY; el->font_size = 28;
    el->start_frame = 100; el->speed = 6;
    safe_strcpy(el->text, sizeof(el->text), "FADE IN TEXT");

    // fade 半透明矩形
    el = &els[n++];
    overlay_opinfo_element_init(el);
    el->type = OPINFO_EL_RECT;
    el->anim = OPINFO_ANIM_FADE;
    el->x = 60; el->y = 540; el->w = 240; el->h = 40;
    el->color = 0xFFFF9500u;
    el->start_frame = 110; el->speed = 6;

    p->elements = els;
    p->element_count = n;
    return 0;
}

// ---------------------------------------------------------------------------
// 5. image 类型：静态整图 + duration 滑入
// ---------------------------------------------------------------------------
static int build_image_static(olopinfo_params_t* p){
    p->type = OPINFO_TYPE_IMAGE;
    p->duration = 800 * 1000;
    safe_strcpy(p->image_path, sizeof(p->image_path), respath("fallback/pr_glitch.jpg"));
    return overlay_opinfo_build_image_elements(p);
}

// ---------------------------------------------------------------------------

typedef struct {
    const char* name;
    int (*build)(olopinfo_params_t* p);
} overlay_sim_preset_t;

static const overlay_sim_preset_t k_presets[OVERLAY_SIM_PRESET_COUNT] = {
    { "arknights-default", build_arknights_default },
    { "arknights-custom",  build_arknights_custom },
    { "custom-text-fx",    build_custom_text_fx },
    { "custom-motion",     build_custom_motion },
    { "image-static",      build_image_static },
};

const char* overlay_sim_preset_name(int idx){
    if(idx < 0 || idx >= OVERLAY_SIM_PRESET_COUNT) return "?";
    return k_presets[idx].name;
}

int overlay_sim_preset_build(int idx, olopinfo_params_t* params){
    if(idx < 0 || idx >= OVERLAY_SIM_PRESET_COUNT) return -1;
    memset(params, 0, sizeof(*params));
    params->src_upscale = 1;
    return k_presets[idx].build(params);
}
