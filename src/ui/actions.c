#include "actions.h"
#include "vars.h"
#include <stdint.h>
#include "log.h"
#include "layer_animation.h"
#include "config.h"
#include "stdlib.h"
#include "lvgl_drm_warp.h"
#include "ui.h"

const char *get_var_epass_version(){
    return EPASS_GIT_VERSION;
}
void set_var_epass_version(const char *value){
    return;
}

const char *get_var_sysinfo(){
    return "System Info goes here";
}
void set_var_sysinfo(const char *value){
    return;
}

sw_mode_t get_var_sw_mode(){
    return sw_mode_t_SW_MODE_SEQUENCE;
}
void set_var_sw_mode(sw_mode_t value){
    return;
}

sw_interval_t get_var_sw_interval(){
    return sw_interval_t_SW_INTERVAL_1MIN;
}
void set_var_sw_interval(sw_interval_t value){
    return;
}
int32_t get_var_brightness(){
    return 6;
}
void set_var_brightness(int32_t value){
    return;
}
const char *get_var_nand_label(){
    return "1/128MB";
}
void set_var_nand_label(const char *value){
    return;
}
const char *get_var_sd_label(){
    return "1/2GB";
}
void set_var_sd_label(const char *value){
    return;
}

int32_t get_var_nand_percent(){
    return 20;
}
void set_var_nand_percent(int32_t value){
    return;
}
int32_t get_var_sd_percent(){
    return 20;
}
void set_var_sd_percent(int32_t value){
    return;
}

const char *get_var_displayimg_size_lbl(){
    return "1/2";
}
void set_var_displayimg_size_lbl(const char *value){
    return;
}

void action_op_sel_cb(lv_event_t * e){
    log_info("action_op_sel_cb");
}
void action_show_oplist(lv_event_t * e){
    log_info("action_show_oplist");
}
extern int g_running;
void action_usb_download(lv_event_t * e){
    log_info("action_usb_download");
    g_running = 0;
}

void action_shutdown(lv_event_t * e){
    log_info("action_shutdown");
    system("shutdown -f");
}

static curr_screen_t g_cur_scr;

void action_show_menu(lv_event_t * e){
    log_info("action_show_menu");
    lv_display_t * disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t*)lv_display_get_driver_data(disp);

    // 对于显示位置不一样的屏幕（spinner和oplist），需要进行动画处理
    switch(g_cur_scr){
        case curr_screen_t_SCREEN_OPLIST:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, UI_OPLIST_Y, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            break;
        case curr_screen_t_SCREEN_SPINNER:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            break;
        default:
            break;
    }
    loadScreen(SCREEN_ID_MAINMENU);
}
void action_format_sd_card(lv_event_t * e){
    log_info("action_format_sd_card");
}
void action_show_sysinfo(lv_event_t * e){
    log_info("action_show_sysinfo");
    loadScreen(SCREEN_ID_SYSINFO);
}


// handle key event for spinner screen and back event.
// for ui bringup
void screen_key_event_cb(uint32_t key){
    lv_display_t * disp = lv_display_get_default();
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);

    log_debug("screen_key_event_cb: g_cur_scr = %d, key = %d", g_cur_scr, key);

    if (g_cur_scr != curr_screen_t_SCREEN_SPINNER){
        if(key == LV_KEY_ESC){
            int from_y = 0;
            if(g_cur_scr == curr_screen_t_SCREEN_OPLIST){
                from_y = UI_OPLIST_Y;
            }
            else{
                from_y = 0;
            }
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation,
                DRM_WARPPER_LAYER_UI,
                0, from_y,
                0, SCREEN_HEIGHT,
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_SPINNER);
        }
        return;
    }
    // if spinner screen, handle key event
    switch(key){
        // go to oplist screen
        case LV_KEY_LEFT:
        case LV_KEY_RIGHT:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, UI_OPLIST_Y, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_OPLIST);
            break;
        case LV_KEY_ENTER:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_DISPLAYIMG);
            break;
        case LV_KEY_ESC:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_MAINMENU);
            break;
    }
}

void action_screen_loaded_cb(lv_event_t * e){
    g_cur_scr = (curr_screen_t)(lv_event_get_user_data(e));
    log_info("action_screen_loaded_cb: cur_scr = %d", g_cur_scr);
};

void action_displayimg_key(lv_event_t * e){
    log_info("action_displayimg_key");
}