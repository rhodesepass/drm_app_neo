// 警告页面 专用
#include "screens.h"
#include "ui.h"
#include "ui/actions_warning.h"
#include "utils/log.h"
#include "ui/scr_transition.h"
#include "config.h"
#include "utils/spsc_queue.h"
#include "lvgl.h"
#include "icons.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <utils/timer.h>

static spsc_bq_t g_warning_queue;
static char g_warning_title[64] = {0};
static char g_warning_desc[64] = {0};
static char g_warning_icon[16] = {0};

static lv_timer_t * g_warning_timer = NULL;
static uint32_t g_last_trigger_tick = 0;
static bool g_queue_initialized = false;
// =========================================
// 自己添加的方法 START
// =========================================

inline static char *get_warning_title(warning_type_t type){
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
        case UI_WARNING_APP_NO_DIRECT_START:
            return "APP不支持直接启动";
        case UI_WARNING_APP_LOAD_ERROR:
            return "部分APP加载失败";
        default:
            return "未知错误";
    }
}

inline static char *get_warning_desc(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return "请尽快将您的通行认证终端连接至电源适配器。";
        case UI_WARNING_ASSET_ERROR:
            return "请根据日志排查干员素材格式问题";
        case UI_WARNING_SD_MOUNT_ERROR:
            return "请检查SD卡格式为FAT32，或进行格式化。";
        case UI_WARNING_PRTS_CONFLICT:
            return "正在切换干员，请稍候重试。";
        case UI_WARNING_NO_ASSETS:
            return "请向您的通行认证终端下装干员素材。";
        case UI_WARNING_NOT_IMPLEMENTED:
            return "我还没写这个功能，要不来git看看帮写写？";
        case UI_WARNING_APP_NO_DIRECT_START:
            return "请通过文件管理器选择此APP支持的文件";
        case UI_WARNING_APP_LOAD_ERROR:
            return "请根据日志检查APP配置文件是否正确";
        default:
            return "为什么你能看到这个告警页面？";
    }
}

inline static char *get_warning_icon(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return UI_ICON_BATTERY_EMPTY;
        case UI_WARNING_ASSET_ERROR:
            return UI_ICON_TRIANGLE_EXCLAMATION;
        case UI_WARNING_SD_MOUNT_ERROR:
            return UI_ICON_SD_CARD;
        case UI_WARNING_PRTS_CONFLICT:
            return UI_ICON_CAR_BURST;
        case UI_WARNING_NO_ASSETS:
            return UI_ICON_BORDER_NONE;
        case UI_WARNING_NOT_IMPLEMENTED:
            return UI_ICON_CODE_PULL_REQUEST;
        case UI_WARNING_APP_NO_DIRECT_START:
            return UI_ICON_TRIANGLE_EXCLAMATION;
        case UI_WARNING_APP_LOAD_ERROR:
            return UI_ICON_TRIANGLE_EXCLAMATION;
        default:
            return UI_ICON_QUESTION;
    }
}

inline static uint32_t get_warning_color(warning_type_t type){
    switch(type){
        case UI_WARNING_LOW_BATTERY:
            return UI_COLOR_ERROR;
        case UI_WARNING_ASSET_ERROR:
            return UI_COLOR_WARNING;
        case UI_WARNING_SD_MOUNT_ERROR:
            return UI_COLOR_ERROR;
        case UI_WARNING_PRTS_CONFLICT:
            return UI_COLOR_WARNING;
        case UI_WARNING_NO_ASSETS:
            return UI_COLOR_WARNING;
        case UI_WARNING_NOT_IMPLEMENTED:
            return UI_COLOR_ERROR;
        case UI_WARNING_APP_NO_DIRECT_START:
            return UI_COLOR_WARNING;
        case UI_WARNING_APP_LOAD_ERROR:
            return UI_COLOR_WARNING;
        default:
            return UI_COLOR_INFO;
    }
}

static void delayed_warning_cb(void* userdata,bool is_last){
    ui_warning((warning_type_t)userdata);
}

// 由外部任意线程请求
void ui_warning(warning_type_t type){
    if(!g_queue_initialized){
        log_warn("queue not initialized, delaying");
        //队列还没有初始化的话 就先塞进timer
        prts_timer_handle_t warning_handle;
        prts_timer_create(&warning_handle, 
            5 * 1000 * 1000, 
            0, 
            1, 
            delayed_warning_cb, 
            (void *)type
        );
        return;
    }
    warning_info_t* info = malloc(sizeof(warning_info_t));
    if(info == NULL){
        log_error("failed to allocate memory for warning info");
        return;
    }
    memset(info, 0, sizeof(warning_info_t));
    info->title = get_warning_title(type);
    info->desc = get_warning_desc(type);
    info->icon = get_warning_icon(type);
    info->color = get_warning_color(type);
    info->str_on_heap = false;
    spsc_bq_push(&g_warning_queue, (void *)info);
}

void ui_warning_custom(char* title, char* desc, char* icon, uint32_t color){
    warning_info_t* info = malloc(sizeof(warning_info_t));
    if(info == NULL){
        log_error("failed to allocate memory for warning info");
        return;
    }
    memset(info, 0, sizeof(warning_info_t));
    info->title = strdup(title);
    info->desc = strdup(desc);
    info->icon = strdup(icon);
    info->color = color;
    info->str_on_heap = true;
    spsc_bq_push(&g_warning_queue, (void *)info);
}
// 由LVGL timer回调触发。
// 别忘记：LVGL不是线程安全的。UI的读写只能由LVGL线程完成。
static void ui_warning_timer_cb(lv_timer_t * timer){
    warning_info_t* info;
    if(lv_tick_get() - g_last_trigger_tick < UI_WARNING_DISPLAY_DURATION / 1000){
        return;
    }
    if(spsc_bq_try_pop(&g_warning_queue, (void **)&info) == 0){
        strcpy(g_warning_title, info->title);
        strcpy(g_warning_desc, info->desc);
        strcpy(g_warning_icon, info->icon);
        lv_obj_set_style_bg_color(
            objects.warning, 
            lv_color_hex(info->color),
            LV_PART_MAIN | LV_STATE_DEFAULT
            );
        if(info->str_on_heap){
            free(info->title);
            free(info->desc);
            free(info->icon);
        }
        free(info);
        ui_schedule_screen_transition(curr_screen_t_SCREEN_WARNING);
        g_last_trigger_tick = lv_tick_get();
    }
}

void ui_warning_init(){
    log_info("==> UI Warning Initializing...");
    spsc_bq_init(&g_warning_queue, 10);
    g_warning_timer = lv_timer_create(ui_warning_timer_cb, UI_WARNING_TIMER_TICK_PERIOD / 1000, NULL);
    g_last_trigger_tick = lv_tick_get();
    g_queue_initialized = true;
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
    return g_warning_title;
}

void set_var_warning_title(const char *value){
    return;
}

const char *get_var_warning_desc(){
    return g_warning_desc;
}

void set_var_warning_desc(const char *value){
    return;
}

const char *get_var_warning_icon(){
    return g_warning_icon;
}
void set_var_warning_icon(const char *value){
    return;
}