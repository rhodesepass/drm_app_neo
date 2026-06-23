//
// ui_services 设备实现 —— 跨线程服务桥 + 平台钩子强符号。
//
// 告警/确认来自任意线程 (prts/apps/battery/main/ipc)；LVGL 非线程安全 ⇒ 入 spsc 队列，
// 由 LVGL 线程上的 lv_timer 取出后再切屏。逻辑搬自原 actions_warning/confirm.c。
//
#include "ui_screens/ui_services.h"
#include "ui_screens/ui_backend.h"
#include "ui_screens/screen_manager.h"
#include "ui_screens/screens/screen_warning.h"
#include "ui_screens/screens/screen_confirm.h"

#include <lvgl/lvgl.h>
#include "config.h"
#include "icons.h"
#include "utils/spsc_queue.h"
#include "utils/log.h"

#include <stdlib.h>
#include <string.h>

extern int g_running;
extern int g_exitcode;

// ui_backend_device 内部动作 (强制显图)
extern void ui_backend_dispimg_force(const char *path);

// ================= 枚举映射 curr_screen_t -> screen_id_t =================
// 两套枚举值不同 (curr: WARNING=7,CONFIRM=8,APPLIST=9; screen_id: APPLIST=7,WARNING=8,CONFIRM=9)。
static screen_id_t map_screen(curr_screen_t s)
{
    switch (s) {
        case curr_screen_t_SCREEN_MAINMENU:   return SCREEN_MAINMENU;
        case curr_screen_t_SCREEN_OPLIST:     return SCREEN_OPLIST;
        case curr_screen_t_SCREEN_SYSINFO:    return SCREEN_SYSINFO;
        case curr_screen_t_SCREEN_SPINNER:    return SCREEN_SPINNER;
        case curr_screen_t_SCREEN_DISPLAYIMG: return SCREEN_DISPLAYIMG;
        case curr_screen_t_SCREEN_FILEMANAGER:return SCREEN_FILEMANAGER;
        case curr_screen_t_SCREEN_SETTINGS:   return SCREEN_SETTINGS;
        case curr_screen_t_SCREEN_WARNING:    return SCREEN_WARNING;
        case curr_screen_t_SCREEN_CONFIRM:    return SCREEN_CONFIRM;
        case curr_screen_t_SCREEN_APPLIST:    return SCREEN_APPLIST;
        default:                              return SCREEN_MAINMENU;
    }
}
static curr_screen_t unmap_screen(screen_id_t s)
{
    switch (s) {
        case SCREEN_MAINMENU:   return curr_screen_t_SCREEN_MAINMENU;
        case SCREEN_OPLIST:     return curr_screen_t_SCREEN_OPLIST;
        case SCREEN_SYSINFO:    return curr_screen_t_SCREEN_SYSINFO;
        case SCREEN_SPINNER:    return curr_screen_t_SCREEN_SPINNER;
        case SCREEN_DISPLAYIMG: return curr_screen_t_SCREEN_DISPLAYIMG;
        case SCREEN_FILEMANAGER:return curr_screen_t_SCREEN_FILEMANAGER;
        case SCREEN_SETTINGS:   return curr_screen_t_SCREEN_SETTINGS;
        case SCREEN_WARNING:    return curr_screen_t_SCREEN_WARNING;
        case SCREEN_CONFIRM:    return curr_screen_t_SCREEN_CONFIRM;
        case SCREEN_APPLIST:    return curr_screen_t_SCREEN_APPLIST;
        default:                return curr_screen_t_SCREEN_MAINMENU;
    }
}

// ================= 告警文案/图标/颜色表 (原 actions_warning.c) =================
static const char *warn_title(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_LOW_BATTERY:          return "电池电量严重不足";
        case UI_WARNING_ASSET_ERROR:          return "部分干员加载失败";
        case UI_WARNING_SD_MOUNT_ERROR:       return "SD卡挂载失败";
        case UI_WARNING_PRTS_CONFLICT:        return "PRTS冲突";
        case UI_WARNING_NO_ASSETS:            return "没有干员素材";
        case UI_WARNING_NOT_IMPLEMENTED:      return "未实现的功能";
        case UI_WARNING_APP_NO_DIRECT_START:  return "APP不支持直接启动";
        case UI_WARNING_APP_LOAD_ERROR:       return "部分APP加载失败";
        case UI_WARNING_APP_ALREADY_RUNNING:  return "APP已经在后台运行";
        default:                              return "未知错误";
    }
}
static const char *warn_desc(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_LOW_BATTERY:          return "请尽快将您的通行认证终端连接至电源适配器。";
        case UI_WARNING_ASSET_ERROR:          return "请根据日志排查干员素材格式问题";
        case UI_WARNING_SD_MOUNT_ERROR:       return "请检查SD卡格式为FAT32，或进行格式化。";
        case UI_WARNING_PRTS_CONFLICT:        return "正在切换干员，请稍候重试。";
        case UI_WARNING_NO_ASSETS:            return "请向您的通行认证终端下装干员素材。";
        case UI_WARNING_NOT_IMPLEMENTED:      return "我还没写这个功能，要不来git看看帮写写？";
        case UI_WARNING_APP_NO_DIRECT_START:  return "请通过文件管理器选择此APP支持的文件";
        case UI_WARNING_APP_LOAD_ERROR:       return "请根据日志检查APP配置文件是否正确";
        case UI_WARNING_APP_ALREADY_RUNNING:  return "此APP已在后台运行，可在应用列表界面关闭。";
        default:                              return "为什么你能看到这个告警页面？";
    }
}
static const char *warn_icon(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_LOW_BATTERY:          return UI_ICON_BATTERY_EMPTY;
        case UI_WARNING_ASSET_ERROR:          return UI_ICON_TRIANGLE_EXCLAMATION;
        case UI_WARNING_SD_MOUNT_ERROR:       return UI_ICON_SD_CARD;
        case UI_WARNING_PRTS_CONFLICT:        return UI_ICON_CAR_BURST;
        case UI_WARNING_NO_ASSETS:            return UI_ICON_BORDER_NONE;
        case UI_WARNING_NOT_IMPLEMENTED:      return UI_ICON_CODE_PULL_REQUEST;
        case UI_WARNING_APP_NO_DIRECT_START:  return UI_ICON_TRIANGLE_EXCLAMATION;
        case UI_WARNING_APP_LOAD_ERROR:       return UI_ICON_TRIANGLE_EXCLAMATION;
        case UI_WARNING_APP_ALREADY_RUNNING:  return UI_ICON_CAR_BURST;
        default:                              return UI_ICON_QUESTION;
    }
}
static uint32_t warn_color(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_LOW_BATTERY:
        case UI_WARNING_SD_MOUNT_ERROR:
        case UI_WARNING_NOT_IMPLEMENTED:      return UI_COLOR_ERROR;
        case UI_WARNING_ASSET_ERROR:
        case UI_WARNING_PRTS_CONFLICT:
        case UI_WARNING_NO_ASSETS:
        case UI_WARNING_APP_NO_DIRECT_START:
        case UI_WARNING_APP_LOAD_ERROR:
        case UI_WARNING_APP_ALREADY_RUNNING:  return UI_COLOR_WARNING;
        default:                              return UI_COLOR_INFO;
    }
}

// ================= 告警队列 =================
typedef struct {
    char    *title, *desc, *icon;
    uint32_t color;
    bool     on_heap;
} warn_info_t;

static spsc_bq_t   s_warn_q;
static lv_timer_t *s_warn_timer;
static uint32_t    s_warn_last_tick;
static bool        s_inited;

void ui_warning(warning_type_t type)
{
    if (!s_inited) { log_warn("ui_warning before init, dropped (type=%d)", type); return; }
    warn_info_t *info = calloc(1, sizeof(*info));
    if (!info) return;
    info->title   = (char *)warn_title(type);
    info->desc    = (char *)warn_desc(type);
    info->icon    = (char *)warn_icon(type);
    info->color   = warn_color(type);
    info->on_heap = false;
    spsc_bq_push(&s_warn_q, info);
}

void ui_warning_custom(char *title, char *desc, char *icon, uint32_t color)
{
    if (!s_inited) return;
    warn_info_t *info = calloc(1, sizeof(*info));
    if (!info) return;
    info->title   = strdup(title);
    info->desc    = strdup(desc);
    info->icon    = strdup(icon);
    info->color   = color;
    info->on_heap = true;
    spsc_bq_push(&s_warn_q, info);
}

static void warn_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (lv_tick_get() - s_warn_last_tick < UI_WARNING_DISPLAY_DURATION / 1000) return;
    warn_info_t *info;
    if (spsc_bq_try_pop(&s_warn_q, (void **)&info) != 0) return;
    screen_warning_show(info->icon, info->title, info->desc, info->color);
    if (info->on_heap) { free(info->title); free(info->desc); free(info->icon); }
    free(info);
    s_warn_last_tick = lv_tick_get();
}

// ================= 确认队列 =================
static spsc_bq_t   s_confirm_q;
static lv_timer_t *s_confirm_timer;

static void proceed_format_sd(void) { ui_hook_format_sd(); }
static void proceed_shutdown(void)  { g_running = 0; g_exitcode = EXITCODE_SHUTDOWN; }

void ui_confirm(ui_confirm_type_t type)
{
    if (!s_inited) return;
    spsc_bq_push(&s_confirm_q, (void *)(intptr_t)type);
}

static void confirm_timer_cb(lv_timer_t *t)
{
    (void)t;
    void *raw;
    if (spsc_bq_try_pop(&s_confirm_q, &raw) != 0) return;
    ui_confirm_type_t type = (ui_confirm_type_t)(intptr_t)raw;
    if (type == UI_CONFIRM_TYPE_FORMAT_SD_CARD)
        screen_confirm_show("确定格式化SD卡吗？", proceed_format_sd);
    else if (type == UI_CONFIRM_TYPE_SHUTDOWN)
        screen_confirm_show("确定要关机吗？", proceed_shutdown);
}

// ================= 切屏 / 显图 / 查询 (供 IPC) =================
void ui_schedule_screen_transition(curr_screen_t to_screen)
{
    screen_show(map_screen(to_screen));
}
void ui_displayimg_force_dispimg(const char *path)
{
    ui_backend_dispimg_force(path);
}
curr_screen_t ui_get_current_screen(void) { return unmap_screen(screens_current()); }
bool          ui_is_hidden(void)          { return screens_current() == SCREEN_SPINNER; }

// ================= 平台钩子强符号 (覆盖 screen_manager 的弱默认) =================
void ui_hook_shutdown_request(void) { ui_confirm(UI_CONFIRM_TYPE_SHUTDOWN); }
void ui_hook_displayimg_key(uint32_t key) { ui_backend_displayimg_key(key); }
void ui_hook_restart(void)     { g_running = 0; g_exitcode = EXITCODE_RESTART_APP; }
void ui_hook_format_sd(void)   { g_running = 0; g_exitcode = EXITCODE_FORMAT_SD_CARD; }
void ui_hook_srgn_config(void) { g_running = 0; g_exitcode = EXITCODE_SRGN_CONFIG; }

// ================= 生命周期 =================
void ui_services_init(void)
{
    spsc_bq_init(&s_warn_q, 10);
    spsc_bq_init(&s_confirm_q, 10);
    s_warn_timer    = lv_timer_create(warn_timer_cb,    UI_WARNING_TIMER_TICK_PERIOD / 1000, NULL);
    s_confirm_timer = lv_timer_create(confirm_timer_cb, UI_WARNING_TIMER_TICK_PERIOD / 1000, NULL);
    s_warn_last_tick = lv_tick_get();
    s_inited = true;
    log_info("==> ui_services initialized");
}
void ui_services_destroy(void)
{
    s_inited = false;
    if (s_warn_timer)    lv_timer_delete(s_warn_timer);
    if (s_confirm_timer) lv_timer_delete(s_confirm_timer);
    spsc_bq_destroy(&s_warn_q);
    spsc_bq_destroy(&s_confirm_q);
}
