// 警告页面 专用
#include "ui.h"
#include "ui/actions_warning.h"
#include "utils/log.h"
#include "ui/scr_transition.h"
#include "config.h"
#include "utils/spsc_queue.h"
#include "lvgl.h"

static spsc_bq_t g_warning_queue;
static warning_type_t g_warning_type = UI_WARNING_NONE;
static lv_timer_t * g_warning_timer = NULL;
// =========================================
// 自己添加的方法 START
// =========================================

inline static const char *get_warning_title(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return "电池电量严重不足";
        case UI_WARNING_ASSET_ERROR:
            return "部分干员加载失败";
        case UI_WARNING_SD_MOUNT_ERROR:
            return "SD卡挂载失败";
        case UI_WARNING_PRTS_CONFLICT:
            return "PRTS冲突";
        case UI_WARNING_NO_ASSETS:
            return "没有干员素材";
        case UI_WARNING_NOT_IMPLEMENTED:
            return "未实现的功能";
        default:
            return "未知错误";
    }
}

inline static const char *get_warning_desc(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return "请尽快将您的通行认证终端连接至电源适配器。";
        case UI_WARNING_ASSET_ERROR:
            return“请根据PRTS_OPERATOR_PARSE_LOG“排查干员素材";
        case UI_WARNING_SD_MOUNT_ERROR:
            return "请检查SD卡格式为FAT32，或进行格式化。";
        case UI_WARNING_PRTS_CONFLICT:
            return "正在切换干员，请稍候重试。";
        case UI_WARNING_NO_ASSETS:
            return "请向您的通行认证终端下装干员素材。";
        case UI_WARNING_NOT_IMPLEMENTED:
            return "我还没写这个功能，要不来git看看帮写写？";
        default:
            return "为什么你能看到这个告警页面？";
    }
}


// 由外部任意线程请求
void ui_warning(warning_type_t type){
    spsc_bq_push(&g_warning_queue, (void *)type);
}

static uint32_t g_last_trigger_tick = 0;
// 由LVGL timer回调触发。
// 别忘记：LVGL不是线程安全的。UI的读写只能由LVGL线程完成。
static void ui_warning_timer_cb(lv_timer_t * timer){
    warning_type_t type;
    if(lv_tick_get() - g_last_trigger_tick < UI_WARNING_DISPLAY_DURATION / 1000){
        return;
    }
    if(spsc_bq_try_pop(&g_warning_queue, (void **)&type) == 0){
        log_debug("ui_warning_timer_cb: type = %d", type);
        g_warning_type = type;
        ui_schedule_screen_transition(curr_screen_t_SCREEN_WARNING);
        g_last_trigger_tick = lv_tick_get();
    }
}

void ui_warning_init(){
    log_info("==> UI Warning Initializing...");
    spsc_bq_init(&g_warning_queue, 10);
    g_warning_timer = lv_timer_create(ui_warning_timer_cb, UI_WARNING_TIMER_TICK_PERIOD / 1000, NULL);
    log_info("==> UI Warning Initialized!");
}
void ui_warning_destroy(){
    spsc_bq_destroy(&g_warning_queue);
    lv_timer_delete(g_warning_timer);
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
