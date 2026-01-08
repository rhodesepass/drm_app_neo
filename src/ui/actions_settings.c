//  设置页面 专用
#include "ui.h"
#include "ui/actions_settings.h"
#include "utils/settings.h"
#include "utils/log.h"

extern objects_t objects;
extern settings_t g_settings;

// =========================================
// 自己添加的方法 START
// =========================================
void ui_settings_load_ctrl_word(){
    if(g_settings.ctrl_word.lowbat_trip){
        lv_obj_add_state(objects.lowbat_trip, LV_STATE_CHECKED);
    }
    else{
        lv_obj_remove_state(objects.lowbat_trip, LV_STATE_CHECKED);
    }
    if(g_settings.ctrl_word.no_intro_block){
        lv_obj_add_state(objects.no_intro_block, LV_STATE_CHECKED);
    }
    else{
        lv_obj_remove_state(objects.no_intro_block, LV_STATE_CHECKED);
    }
    if(g_settings.ctrl_word.no_overlay_block){
        lv_obj_add_state(objects.no_overlay_block, LV_STATE_CHECKED);
    }
    else{
        lv_obj_remove_state(objects.no_overlay_block, LV_STATE_CHECKED);
    }

}


// =========================================
// EEZ 回调 START
// =========================================


void action_settings_ctrl_changed(lv_event_t * e){
    log_debug("action_settings_ctrl_changed");
    g_settings.ctrl_word.lowbat_trip = lv_obj_has_state(objects.lowbat_trip, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_intro_block = lv_obj_has_state(objects.no_intro_block, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_overlay_block = lv_obj_has_state(objects.no_overlay_block, LV_STATE_CHECKED);
    settings_update(&g_settings);
    return;
}

sw_mode_t get_var_sw_mode(){
    return g_settings.switch_mode;
}
void set_var_sw_mode(sw_mode_t value){
    g_settings.switch_mode = value;
    settings_update(&g_settings);
    return;
}

sw_interval_t get_var_sw_interval(){
    return g_settings.switch_interval;
}
void set_var_sw_interval(sw_interval_t value){
    g_settings.switch_interval = value;
    settings_update(&g_settings);
    return;
}
int32_t get_var_brightness(){
    return g_settings.brightness;
}
void set_var_brightness(int32_t value){
    g_settings.brightness = value;
    settings_update(&g_settings);
    return;
}

usb_mode_t get_var_usb_mode(){
    return g_settings.usb_mode;
}
void set_var_usb_mode(usb_mode_t value){
    g_settings.usb_mode = value;
    settings_update(&g_settings);
    settings_set_usb_mode(value);
    return;
}
