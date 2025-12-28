#include "lvgl.h"
#include "overlays.h"
#include "log.h"
#include "config.h"

lv_obj_t * ui_create_overlay_image(){
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t * img = lv_img_create(scr);

    lv_img_set_src(img, "A:/assets/MS/overlay.png");

    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    return scr;
}

lv_obj_t * ui_create_overlay_arknights(){
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x00ffcc), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * img_tl_rhodes = lv_img_create(scr);
    lv_img_set_src(img_tl_rhodes, UI_OVERLAY_ARKNIGHTS_PREFIX "top_left_rhodes.png");
    lv_obj_align(img_tl_rhodes, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * img_bl_bar = lv_img_create(scr);
    lv_img_set_src(img_bl_bar, UI_OVERLAY_ARKNIGHTS_PREFIX "btn_left_bar.png");
    lv_obj_align(img_bl_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t * img_br_fade = lv_img_create(scr);
    lv_img_set_src(img_br_fade, UI_OVERLAY_ARKNIGHTS_PREFIX "btn_right_fade.png");
    lv_obj_align(img_br_fade, LV_ALIGN_BOTTOM_RIGHT, 0, 0);


    lv_obj_t * img_tr_bar = lv_img_create(scr);
    lv_img_set_src(img_tr_bar, UI_OVERLAY_ARKNIGHTS_PREFIX "top_right_bar.png");
    lv_obj_align(img_tr_bar, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    lv_obj_t * img_tl_rect = lv_img_create(scr);
    lv_img_set_src(img_tl_rect, UI_OVERLAY_ARKNIGHTS_PREFIX "top_left_rect.png");
    lv_obj_align(img_tl_rect, LV_ALIGN_TOP_LEFT, 60, 0);

    lv_obj_t * img_ak_bar = lv_img_create(scr);
    lv_img_set_src(img_ak_bar, UI_OVERLAY_ARKNIGHTS_PREFIX "ak_bar.png");
    lv_obj_align(img_ak_bar, LV_ALIGN_TOP_LEFT, UI_OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_X, UI_OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y);

    static lv_anim_t tl_rhodes_anim;
    lv_anim_init(&tl_rhodes_anim);
    lv_anim_set_exec_cb(&tl_rhodes_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_set_var(&tl_rhodes_anim, img_tl_rhodes);
    lv_anim_set_values(&tl_rhodes_anim, 0, -600);
    lv_anim_set_time(&tl_rhodes_anim, 60000);
    lv_anim_set_repeat_count(&tl_rhodes_anim, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&tl_rhodes_anim);
    
    return scr;
}