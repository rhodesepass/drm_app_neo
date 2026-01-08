#pragma once 
#include "overlay/overlay.h"



typedef struct {
    // shared by all transitions
    int duration;

    // fade,move,swipe
    char image_path[128];
    int image_w;
    int image_h;
    uint32_t* image_addr;

    uint32_t background_color;
} oltr_params_t;

void overlay_transition_fade(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params);
void overlay_transition_move(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params);
void overlay_transition_swipe(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params);

void overlay_transition_load_image(oltr_params_t* params);
void overlay_transition_free_image(oltr_params_t* params);