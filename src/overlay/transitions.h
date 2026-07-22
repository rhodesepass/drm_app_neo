#pragma once
#include "config.h"
#include "overlay/overlay.h"


typedef enum {
    TRANSITION_TYPE_FADE,
    TRANSITION_TYPE_MOVE,
    TRANSITION_TYPE_SWIPE,
    TRANSITION_TYPE_NONE,
} transition_type_t;

typedef struct {
    // shared by all transitions
    int duration;
    transition_type_t type;

    // fade,move,swipe
    char image_path[128];
    int image_w;
    int image_h;
    uint32_t* image_addr;
    // 图片加载后的最近邻缩放:放大(720p 档显示 360 素材)/缩小(360 档显示 720 素材)。只命中一侧。
    int src_upscale;
    int src_downscale;

    uint32_t background_color;

    // C8: load 阶段攒好的动态段颜色池(bg color + 图片量化结果),
    // 各 transition 入口在层不可见时写表+commit
    uint32_t c8_pool[C8PAL_QUOTA_TRIMG + 4];
    int c8_pool_n;
} oltr_params_t;

typedef struct {
    void (*middle_cb)(void *userdata,bool is_last);
    void* middle_cb_userdata;
    void (*end_cb)(void *userdata,bool is_last);
    void* end_cb_userdata;
    // 需不需要 transition 结束后释放这个结构体
    bool on_heap;
    // end_cb_userdata 是否需要在清理时释放
    bool end_cb_userdata_on_heap;
} oltr_callback_t;

void overlay_transition_fade(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params);
void overlay_transition_move(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params);
void overlay_transition_swipe(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params);

void overlay_transition_load_image(oltr_params_t* params);
void overlay_transition_free_image(oltr_params_t* params);