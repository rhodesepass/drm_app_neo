// 主界面 专用
#include "ui.h"
#include "ui/actions_mainscreen.h"
#include "ui/scr_transition.h"
#include "utils/log.h"
#include "ui/actions_confirm.h"
#include <stdlib.h>
#include "config.h"

// =========================================
// 自己添加的方法 START
// =========================================


// =========================================
// EEZ 回调 START
// =========================================


extern int g_running;
extern int g_exitcode;
void action_restart_app(lv_event_t * e){
    log_debug("action_restart_app");
    g_running = 0;
    g_exitcode = EXITCODE_RESTART_APP;
}

void action_shutdown(lv_event_t * e){
    log_debug("action_shutdown");
    ui_confirm(UI_CONFIRM_TYPE_SHUTDOWN);
}

void action_show_oplist(lv_event_t * e){
    log_debug("action_show_oplist");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_OPLIST);
}
void action_show_sysinfo(lv_event_t * e){
    log_debug("action_show_sysinfo");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_SYSINFO);
}
void action_show_settings(lv_event_t * e){
    log_debug("action_show_settings");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_SETTINGS);
}
void action_show_files(lv_event_t * e){
    log_debug("action_show_files");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_FILEMANAGER);
}

// 应用列表
void action_show_apps(lv_event_t * e){
    log_debug("action_show_apps");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_APPLIST);
}

// 严格来说 它不是mainscreen的回调
// 但是 姑且放在这里把
void action_show_menu(lv_event_t * e){
    log_debug("action_show_menu");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_MAINMENU);
}