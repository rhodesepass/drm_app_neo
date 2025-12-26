#include "lvgl.h"
#include "overlays.h"
#include "log.h"
#include "config.h"

lv_obj_t * ui_create_overlay_image(){
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t * img;

    img = lv_image_create(scr);

    lv_image_set_src(img, "A:/assets/MS/overlay.png");
    int width_src = lv_image_get_src_width(img);
    int height_src = lv_image_get_src_height(img);


    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    return scr;
}

lv_obj_t * ui_create_overlay_arknights(){
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t * img;

    img = lv_image_create(scr);

    lv_image_set_src(img, "A:/assets/MS/overlay.png");
    int width_src = lv_image_get_src_width(img);
    int height_src = lv_image_get_src_height(img);

    
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    return scr;
}