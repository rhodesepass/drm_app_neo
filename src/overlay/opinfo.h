#pragma once

#include "overlay/overlay.h"


typedef struct {
    // 通用参数
    int fade_duration;

    // image 图像类型
    char image_path[128];

    //arknights 带有简单动态效果的明日方舟通行证模板
    char operator_name[20];
    char operator_code[40];
    char barcode_text[20];
    char staff_text[40];
    
    int class_w;
    int class_h;
    uint8_t* class_addr;

    char aux_text[256];

    int logo_w;
    int logo_h;
    uint8_t* logo_addr;

    uint32_t color;

} olopinfo_params_t;

void overlay_opinfo_show_image(overlay_t* overlay,olopinfo_params_t* params);
void overlay_opinfo_show_arknights(overlay_t* overlay,olopinfo_params_t* params);

void overlay_opinfo_stop(overlay_t* overlay);