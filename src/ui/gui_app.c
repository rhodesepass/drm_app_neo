#include "gui_app.h"
#include "lvgl.h"
#include "log.h"
#include "lvgl_drm_warp.h"
#include "overlays.h"
// #include "demos/widgets/lv_demo_widgets.h"
// #include "demos/benchmark/lv_demo_benchmark.h"
static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        LV_LOG_USER("Clicked");
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        LV_LOG_USER("Toggled");
    }
}

void gui_app_create_ui(lvgl_drm_warp_t *lvgl_drm_warp){
    lv_obj_t * label;

    // lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, LV_PART_MAIN);
    lv_disp_set_bg_opa(NULL, LV_OPA_TRANSP);


    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);

    label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    label = lv_label_create(btn2);
    lv_label_set_text(label, "Toggle");
    lv_obj_center(label);

    lv_obj_t * overlay_screen = ui_create_overlay_arknights();
    lv_scr_load_anim(overlay_screen, LV_SCR_LOAD_ANIM_FADE_OUT, 2000, 0, true);
}