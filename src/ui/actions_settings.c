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
    settings_lock(&g_settings);
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
    settings_unlock(&g_settings);
}


// =========================================
// EEZ 回调 START
// =========================================

// 这个按钮目前不起任何作用。它存在的意义仅仅是因为它是ui group的第一个元素。
// 因为：目前设置里的slider，触发切换开/关是绑定在按键的release事件上的。
// 假设用户从mainmenu，按下（press）enter按钮，进入设置界面。
// 这时候用户松开手，按钮的release事件被触发，slider的切换开/关被触发，
// 导致误整定第一个设置。
// lv_btn按钮的触发事件是绑定在press上面的，不会有这个误触发的问题
// 因此我这里直接添加一个没有作用的按钮。
void action_clear_cache(lv_event_t * e){
    log_debug("action_clear_cache");
    // does nothing.
}

void action_settings_ctrl_changed(lv_event_t * e){
    log_debug("action_settings_ctrl_changed");
    settings_lock(&g_settings);
    g_settings.ctrl_word.lowbat_trip = lv_obj_has_state(objects.lowbat_trip, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_intro_block = lv_obj_has_state(objects.no_intro_block, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_overlay_block = lv_obj_has_state(objects.no_overlay_block, LV_STATE_CHECKED);
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    return;
}

sw_mode_t get_var_sw_mode(){
    return g_settings.switch_mode;
}
void set_var_sw_mode(sw_mode_t value){
    settings_lock(&g_settings);
    g_settings.switch_mode = value;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    return;
}

sw_interval_t get_var_sw_interval(){
    return g_settings.switch_interval;
}
void set_var_sw_interval(sw_interval_t value){
    settings_lock(&g_settings);
    g_settings.switch_interval = value;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    return;
}
int32_t get_var_brightness(){
    return g_settings.brightness;
}
void set_var_brightness(int32_t value){
    settings_lock(&g_settings);
    g_settings.brightness = value;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    return;
}

usb_mode_t get_var_usb_mode(){
    return g_settings.usb_mode;
}
void set_var_usb_mode(usb_mode_t value){
    settings_lock(&g_settings);
    g_settings.usb_mode = value;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    settings_set_usb_mode(value);
    return;
}
