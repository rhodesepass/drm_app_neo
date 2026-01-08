#include <src/core/lv_obj_style.h>
#include <stdint.h>
#include <stdlib.h>

#include "actions.h"
#include "screens.h"
#include "vars.h"
#include "utils/log.h"
#include "render/layer_animation.h"
#include "config.h"
#include "render/lvgl_drm_warp.h"
#include "ui.h"
#include "utils/settings.h"
#include "utils/storage.h"
#include "utils/sysinfo.h"
#include "utils/timer.h"
#include "ui/filemanager.h"

extern settings_t g_settings;

const char *get_var_epass_version(){
    return EPASS_GIT_VERSION;
}
void set_var_epass_version(const char *value){
    return;
}


const char *get_var_sysinfo(){
    static char buf[2048];
    char meminfo[512] = {0};
    char osrelease[512] = {0};
    static int called_cnt = 0;
    if(called_cnt != 0){
        return buf;
    }
    called_cnt++;

    get_meminfo_str(meminfo, sizeof(meminfo));
    get_os_release_str(osrelease, sizeof(osrelease));

    uint32_t app_crc32 = get_file_crc32("/root/epass_drm_app");

    snprintf(
        buf, sizeof(buf), 
        "罗德岛电子通行认证程序-代号:%s\n"
        "版本号: %s\n"
        "校验码: %08X\n"
        "程序生成时间: %s\n"
        "%s"
        "%s\n",
        APP_SUBCODENAME,
        EPASS_GIT_VERSION,
        app_crc32,
        COMPILE_TIME,
        meminfo,
        osrelease
    );

    // refresh every 300 calls(10 secs)
    if(called_cnt == 300){
        called_cnt = 0;
    }
    return buf;
}
void set_var_sysinfo(const char *value){
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
const char *get_var_nand_label(){
    static char buf[128];
    snprintf(buf, sizeof(buf), 
        "%d/%dMB", 
        get_nand_available_size() / 1024 / 1024, 
        get_nand_total_size() / 1024 / 1024
    );
    return buf;
}
void set_var_nand_label(const char *value){
    return;
}
const char *get_var_sd_label(){
    static char buf[128];
    if(!is_sdcard_inserted()){
        return "SD卡不存在";
    }
    snprintf(buf, sizeof(buf), 
        "%d/%dMB", 
        get_sd_available_size() / 1024 / 1024, 
        get_sd_total_size() / 1024 / 1024
    );
    return buf;
}
void set_var_sd_label(const char *value){
    return;
}

int32_t get_var_nand_percent(){
    return (get_nand_available_size() * 100) / get_nand_total_size();
}
void set_var_nand_percent(int32_t value){
    return;
}
int32_t get_var_sd_percent(){
    if(!is_sdcard_inserted()){
        return 0;
    }
    return (get_sd_available_size() * 100) / get_sd_total_size();
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

usb_mode_t get_var_usb_mode(){
    return g_settings.usb_mode;
}
void set_var_usb_mode(usb_mode_t value){
    g_settings.usb_mode = value;
    settings_update(&g_settings);
    settings_set_usb_mode(value);
    return;
}


void action_op_sel_cb(lv_event_t * e){
    log_debug("action_op_sel_cb");
}

extern int g_running;
extern int g_exitcode;
void action_restart_app(lv_event_t * e){
    log_debug("action_restart_app");
    g_running = 0;
    g_exitcode = EXITCODE_RESTART_APP;
}

void action_shutdown(lv_event_t * e){
    log_debug("action_shutdown");
    system("poweroff");
}


inline static int get_screen_target_y(curr_screen_t screen){
    switch(screen){
        case curr_screen_t_SCREEN_OPLIST:
            return UI_OPLIST_Y;
        case curr_screen_t_SCREEN_MAINMENU:
            return UI_MAINMENU_Y;
        case curr_screen_t_SCREEN_BATTERY_ALERT:
            return UI_LOWBAT_Y;
        case curr_screen_t_SCREEN_SPINNER:
            return UI_HEIGHT;
        default:
            return 0;
    }
}

inline static void load_screen(curr_screen_t screen){
    switch(screen){
        case curr_screen_t_SCREEN_OPLIST:
            loadScreen(SCREEN_ID_OPLIST);
            break;
        case curr_screen_t_SCREEN_MAINMENU:
            loadScreen(SCREEN_ID_MAINMENU);
            break;
        case curr_screen_t_SCREEN_BATTERY_ALERT:
            loadScreen(SCREEN_ID_BATTERY_ALERT);
            break;
        case curr_screen_t_SCREEN_SPINNER:
            loadScreen(SCREEN_ID_SPINNER);
            break;
        case curr_screen_t_SCREEN_DISPLAYIMG:
            loadScreen(SCREEN_ID_DISPLAYIMG);
            break;
        case curr_screen_t_SCREEN_SETTINGS:
            loadScreen(SCREEN_ID_SETTINGS);
            break;
        case curr_screen_t_SCREEN_SYSINFO:
            loadScreen(SCREEN_ID_SYSINFO);
            break;
        case curr_screen_t_SCREEN_FILEMANAGER:
            loadScreen(SCREEN_ID_FILEMANAGER);
            break;
    }
}

static curr_screen_t g_cur_scr;

static void delayed_load_screen_cb(void* arg, bool is_last){
    curr_screen_t screen = (curr_screen_t)arg;
    load_screen(screen);
}

static void schedule_screen_transition(curr_screen_t to_screen){
    lv_display_t * disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t*)lv_display_get_driver_data(disp);
    
    int current_y = get_screen_target_y(g_cur_scr);
    int target_y = get_screen_target_y(to_screen);

    prts_timer_handle_t timer_handle;

    // 从spinner 到 其他任何屏幕，(除低电量）都做二阶段动画
    if(g_cur_scr == curr_screen_t_SCREEN_SPINNER && to_screen != curr_screen_t_SCREEN_BATTERY_ALERT){
        prts_timer_create(
            &timer_handle, 
            UI_LAYER_ANIMATION_INTRO_LOADSCREEN_DELAY, 
            0, 
            1, 
            delayed_load_screen_cb, 
            (void*)to_screen
        );
        layer_animation_ease_in_out_move(
            lvgl_drm_warp->layer_animation, 
            DRM_WARPPER_LAYER_UI, 
            0, SCREEN_HEIGHT, 
            0, UI_SPINNER_INTRO_Y, 
            UI_LAYER_ANIMATION_INTRO_SPINNER_TRANSITION_DURATION, 0);
    
        layer_animation_ease_in_out_move(
            lvgl_drm_warp->layer_animation, 
            DRM_WARPPER_LAYER_UI, 
            0, UI_SPINNER_INTRO_Y, 
            0, target_y, 
            UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DURATION, 
            UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DELAY);
    }
    else{
        // 其他情况，直接过渡。
        if(current_y != target_y){
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, current_y, 
                0, target_y, 
                UI_LAYER_ANIMATION_DURATION, 0);
        }
        load_screen(to_screen);
    }

}

void action_show_oplist(lv_event_t * e){
    log_debug("action_show_oplist");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    schedule_screen_transition(curr_screen_t_SCREEN_OPLIST);
}

void action_show_menu(lv_event_t * e){
    log_debug("action_show_menu");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    schedule_screen_transition(curr_screen_t_SCREEN_MAINMENU);
}
void action_format_sd_card(lv_event_t * e){
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    log_debug("action_format_sd_card");
}
void action_show_sysinfo(lv_event_t * e){
    log_debug("action_show_sysinfo");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    schedule_screen_transition(curr_screen_t_SCREEN_SYSINFO);
}
void action_show_settings(lv_event_t * e){
    log_debug("action_show_settings");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    schedule_screen_transition(curr_screen_t_SCREEN_SETTINGS);
}
void action_show_files(lv_event_t * e){
    log_debug("action_show_files");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    schedule_screen_transition(curr_screen_t_SCREEN_FILEMANAGER);
}


// 全局按钮回调。主要用于屏幕切换时放过渡。
void screen_key_event_cb(uint32_t key){

    log_debug("screen_key_event_cb: g_cur_scr = %d, key = %d", g_cur_scr, key);

    // 展示扩列图 界面，按下3/4按钮都可关闭
    if (g_cur_scr == curr_screen_t_SCREEN_DISPLAYIMG){
        if(key == LV_KEY_ESC || key == LV_KEY_ENTER){
            schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
        }
        return;
    }

    // 主界面 和 干员列表
    if (g_cur_scr == curr_screen_t_SCREEN_MAINMENU || g_cur_scr == curr_screen_t_SCREEN_OPLIST){
        if(key == LV_KEY_ESC){
            schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
        }
        return;
    }

    // 其他界面
    if (g_cur_scr != curr_screen_t_SCREEN_SPINNER){
        // 不在编辑状态的时候再按下esc 回到主界面
        bool is_editing = lv_group_get_editing(groups.op);
        if(key == LV_KEY_ESC){
            if(!is_editing)
                schedule_screen_transition(curr_screen_t_SCREEN_MAINMENU);
            else
                lv_group_set_editing(groups.op, false);
        }
        return;
    }

    // spinner（空界面），处理ui出现

    switch(key){
        // go to oplist screen
        case LV_KEY_LEFT:
        case LV_KEY_RIGHT:
           schedule_screen_transition(curr_screen_t_SCREEN_OPLIST);
            break;
        case LV_KEY_ENTER:
            schedule_screen_transition(curr_screen_t_SCREEN_DISPLAYIMG);
            break;
        case LV_KEY_ESC:
            schedule_screen_transition(curr_screen_t_SCREEN_MAINMENU);
            break;
    }
}

extern objects_t objects;
static void load_ctrl_word(){
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

void action_settings_ctrl_changed(lv_event_t * e){
    log_debug("action_settings_ctrl_changed");
    g_settings.ctrl_word.lowbat_trip = lv_obj_has_state(objects.lowbat_trip, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_intro_block = lv_obj_has_state(objects.no_intro_block, LV_STATE_CHECKED);
    g_settings.ctrl_word.no_overlay_block = lv_obj_has_state(objects.no_overlay_block, LV_STATE_CHECKED);
    settings_update(&g_settings);
    return;
}

void action_screen_loaded_cb(lv_event_t * e){
    g_cur_scr = (curr_screen_t)(lv_event_get_user_data(e));
    log_debug("action_screen_loaded_cb: cur_scr = %d", g_cur_scr);

    if(g_cur_scr == curr_screen_t_SCREEN_SETTINGS){
        load_ctrl_word();
    }

    if(g_cur_scr == curr_screen_t_SCREEN_FILEMANAGER){
        add_filemanager_to_group();
    }
    return;
};

void action_displayimg_key(lv_event_t * e){
    log_debug("action_displayimg_key");
}