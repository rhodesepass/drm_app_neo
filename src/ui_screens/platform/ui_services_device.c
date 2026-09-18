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
#include "ui/uix_session.h"

#include <lvgl/lvgl.h>
#include "config.h"
#include "icons.h"
#include "utils/spsc_queue.h"
#include "utils/log.h"

#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

extern int g_running;
extern int g_exitcode;

// ui_backend_device 内部动作 (强制显图 / 重扫目录)
extern void ui_backend_dispimg_force(const char *path);
extern void ui_backend_dispimg_rescan(void);

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
        case curr_screen_t_SCREEN_USBSELECT:  return SCREEN_USBSELECT;
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
        case SCREEN_USBSELECT:  return curr_screen_t_SCREEN_USBSELECT;
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
        case UI_WARNING_VIDEO_DECODE_ERROR:   return "素材解码失败";
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
        case UI_WARNING_VIDEO_DECODE_ERROR:   return "视频格式不受支持或文件已损坏。";
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
        case UI_WARNING_VIDEO_DECODE_ERROR:   return UI_ICON_TRIANGLE_EXCLAMATION;
        default:                              return UI_ICON_QUESTION;
    }
}
static uint32_t warn_color(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_LOW_BATTERY:
        case UI_WARNING_SD_MOUNT_ERROR:
        case UI_WARNING_NOT_IMPLEMENTED:
        case UI_WARNING_VIDEO_DECODE_ERROR:   return UI_COLOR_ERROR;
        case UI_WARNING_ASSET_ERROR:
        case UI_WARNING_PRTS_CONFLICT:
        case UI_WARNING_NO_ASSETS:
        case UI_WARNING_APP_NO_DIRECT_START:
        case UI_WARNING_APP_LOAD_ERROR:
        case UI_WARNING_APP_ALREADY_RUNNING:  return UI_COLOR_WARNING;
        default:                              return UI_COLOR_INFO;
    }
}

// 有对应解析日志的告警：装了能开 ".log" 的 APP 时，直接问用户要不要看日志。
static const char *warn_log_path(warning_type_t t)
{
    switch (t) {
        case UI_WARNING_ASSET_ERROR:    return PRTS_OPERATOR_PARSE_LOG;
        case UI_WARNING_APP_LOAD_ERROR: return APPS_PARSE_LOG;
        default:                        return NULL;
    }
}

// ================= 模态占用判定 =================
// 告警只是"通知"，确认/USB选择是"要用户答复"。后者在场时告警必须排队等着，
// 否则会把等答复的弹窗冲掉：UIX 会话会一直挂到超时，本地二次确认的回调直接丢。
static bool modal_busy(void)
{
    screen_id_t cur = screens_current();
    if (cur == SCREEN_CONFIRM || cur == SCREEN_USBSELECT) return true;
    // 弹屏请求已发出但 ipc_helper 的 timer 还没跑到，此刻屏还是旧的
    return uix_session_pending();
}

// ================= 告警队列 =================
typedef struct {
    char    *title, *desc, *icon;
    uint32_t color;
    bool     on_heap;
    const char *log_path;   // 非 NULL 且有关联 APP 时改走 confirm
} warn_info_t;

static spsc_bq_t   s_confirm_q;   // 定义在前:告警要看它排没排队
static spsc_bq_t   s_warn_q;
static lv_timer_t *s_warn_timer;
static uint32_t    s_warn_last_tick;
static bool        s_inited;

// prts/apps 的首次扫描在 main 里,早于 ui_services_init(LVGL 起来才建队列),
// 开机就报错的告警会全丢。先记下来,init 时补投。此时只有 main 线程在跑,不用加锁。
#define WARN_PENDING_MAX 4
static warning_type_t s_pending_warn[WARN_PENDING_MAX];
static int            s_pending_warn_count;

void ui_warning(warning_type_t type)
{
    if (!s_inited) {
        for (int i = 0; i < s_pending_warn_count; i++)
            if (s_pending_warn[i] == type) return;
        if (s_pending_warn_count < WARN_PENDING_MAX)
            s_pending_warn[s_pending_warn_count++] = type;
        else
            log_warn("ui_warning before init, dropped (type=%d)", type);
        return;
    }
    warn_info_t *info = calloc(1, sizeof(*info));
    if (!info) return;
    info->title   = (char *)warn_title(type);
    info->desc    = (char *)warn_desc(type);
    info->icon    = (char *)warn_icon(type);
    info->color   = warn_color(type);
    info->on_heap = false;
    info->log_path = warn_log_path(type);
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

// confirm 的回调没有参数，待打开的日志只能先存下来
static char s_pending_log[128];
static void proceed_open_log(void) { ui_backend_open_log(s_pending_log); }

static void warn_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (modal_busy()) return;
    // 确认已排队但还没弹出来:让它先弹,否则告警插队后又会被确认盖掉
    if (spsc_bq_count(&s_confirm_q) > 0) return;
    if (lv_tick_get() - s_warn_last_tick < UI_WARNING_DISPLAY_DURATION / 1000) return;
    warn_info_t *info;
    if (spsc_bq_try_pop(&s_warn_q, (void **)&info) != 0) return;
    if (info->log_path && ui_backend_log_viewer_available()) {
        lv_strlcpy(s_pending_log, info->log_path, sizeof(s_pending_log));
        screen_confirm_show2(info->title, "是否打开日志查看？", proceed_open_log, NULL);
    } else {
        screen_warning_show(info->icon, info->title, info->desc, info->color);
    }
    if (info->on_heap) { free(info->title); free(info->desc); free(info->icon); }
    free(info);
    s_warn_last_tick = lv_tick_get();
}

// ================= 确认队列 =================
static lv_timer_t *s_confirm_timer;

static void proceed_format_sd(void) { ui_hook_format_sd(); }
static atomic_bool s_shutdown_pending;
static void proceed_shutdown(void)  { g_running = 0; g_exitcode = EXITCODE_SHUTDOWN; }
static void cancel_shutdown(void) { atomic_store(&s_shutdown_pending, false); }

void ui_confirm(ui_confirm_type_t type)
{
    if (!s_inited) return;
    if (type == UI_CONFIRM_TYPE_SHUTDOWN && atomic_exchange(&s_shutdown_pending, true)) return;
    if (spsc_bq_push(&s_confirm_q, (void *)(intptr_t)type) != 0 && type == UI_CONFIRM_TYPE_SHUTDOWN)
        atomic_store(&s_shutdown_pending, false);
}

static void confirm_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (modal_busy()) return;
    void *raw;
    if (spsc_bq_try_pop(&s_confirm_q, &raw) != 0) return;
    ui_confirm_type_t type = (ui_confirm_type_t)(intptr_t)raw;
    if (type == UI_CONFIRM_TYPE_FORMAT_SD_CARD)
        screen_confirm_show("确定格式化数据盘吗？", proceed_format_sd);
    else if (type == UI_CONFIRM_TYPE_SHUTDOWN)
        screen_confirm_show_shutdown(proceed_shutdown, cancel_shutdown);
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
void ui_displayimg_rescan(void)
{
    ui_backend_dispimg_rescan();
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

    int pending = s_pending_warn_count;
    s_pending_warn_count = 0;
    for (int i = 0; i < pending; i++)
        ui_warning(s_pending_warn[i]);
}
void ui_services_destroy(void)
{
    s_inited = false;
    if (s_warn_timer)    lv_timer_delete(s_warn_timer);
    if (s_confirm_timer) lv_timer_delete(s_confirm_timer);
    spsc_bq_destroy(&s_warn_q);
    spsc_bq_destroy(&s_confirm_q);
}
