// 警告页面 专用
#include "ui.h"
#include "ui/actions_warning.h"
#include "utils/log.h"
#include "ui/scr_transition.h"

static warning_type_t g_warning_type = UI_WARNING_NONE;

// =========================================
// 自己添加的方法 START
// =========================================

inline static const char *get_warning_title(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return "电池电量严重不足";
        case UI_WARNING_ASSET_ERROR:
            return "部分干员加载失败";
        default:
            return "未知错误";
    }
}

inline static const char *get_warning_desc(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return "请尽快将您的通行认证终端连接至电源适配器。";
        case UI_WARNING_ASSET_ERROR:
            return "请根据app/asset.log排查干员素材格式问题";
        default:
            return "为什么你能看到这个告警页面？";
    }
}

void ui_warning(warning_type_t type){
    g_warning_type = type;
    ui_schedule_screen_transition(curr_screen_t_SCREEN_WARNING);
}

// =========================================
// EEZ 回调 START
// =========================================


const char *get_var_warning_title(){
    return get_warning_title(g_warning_type);
}

void set_var_warning_title(const char *value){
    return;
}

const char *get_var_warning_desc(){
    return get_warning_desc(g_warning_type);
}

void set_var_warning_desc(const char *value){
    return;
}
