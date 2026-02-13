#pragma once

#include "overlay/overlay.h"

typedef enum {
    OPINFO_TYPE_IMAGE,
    OPINFO_TYPE_ARKNIGHTS,
    OPINFO_TYPE_NONE,
} opinfo_type_t;

typedef struct {
    opinfo_type_t type;

    // 通用参数
    int appear_time;

    // image 图像类型
    int duration; // 图像进入时的动画时长
    char image_path[128];
    int image_w;
    int image_h;
    uint32_t* image_addr;

    //arknights 带有简单动态效果的明日方舟通行证模板
    char operator_name[20];
    char operator_code[40];
    char barcode_text[40];
    char staff_text[40];
    
    char class_path[128];
    int class_w;
    int class_h;
    uint32_t* class_addr;

    char aux_text[256];

    char logo_path[128];
    int logo_w;
    int logo_h;
    uint32_t* logo_addr;

    char rhodes_text[40];        // 用户自定义文字，非空时替代默认 rhodes logo 图片

    uint32_t color;

} olopinfo_params_t;


void overlay_opinfo_load_image(olopinfo_params_t* params);
void overlay_opinfo_free_image(olopinfo_params_t* params);

void overlay_opinfo_show_image(overlay_t* overlay,olopinfo_params_t* params);
void overlay_opinfo_show_arknights(overlay_t* overlay,olopinfo_params_t* params);
