//
// ui_backend 设备实现 —— 把手写 UI 的数据/动作 seam 接到真实子系统:
// settings(g_settings) / prts(干员) / apps(应用) / statvfs / 扩列图目录扫描。
// 逻辑搬自原 actions_settings/sysinfo/displayimg/oplist/apps。
//
#include "ui_screens/ui_backend.h"
#include "ui_screens/ui_services.h"
#include "ui/ui_theme.h"

#include <lvgl/lvgl.h>   // LV_KEY_*
#include "config.h"
#include "prts/prts.h"
#include "apps/apps.h"
#include "apps/apps_types.h"
#include "utils/settings.h"
#include "utils/log.h"
#include "utils/misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/statvfs.h>
#include <dirent.h>

extern settings_t g_settings;
extern bool g_use_sd;

static prts_t *s_prts;
static apps_t *s_apps;

// ================= 扩列图 (原 actions_displayimg.c) =================
typedef struct {
    char img_path[DISPLAYIMG_MAX_PATH_LENGTH];
    bool is_gif;
} dispimg_file_t;

static struct {
    dispimg_file_t files[DISPLAYIMG_MAX_COUNT];
    int  count;
    int  index;
    char forced[DISPLAYIMG_MAX_PATH_LENGTH]; // IPC 强制显图 (优先于列表)
    bool forced_is_gif;
    bool forced_active;
} s_di;

static int dispimg_type(const char *name) // 0=jpg 1=png 2=bmp 3=gif -1=非法
{
    if (!name) return -1;
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return -1;
    char ext[8] = {0};
    dot++;
    for (int i = 0; dot[i] && i < 6; i++)
        ext[i] = (dot[i] >= 'A' && dot[i] <= 'Z') ? (dot[i] + 32) : dot[i];
    if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg")) return 0;
    if (!strcmp(ext, "png")) return 1;
    if (!strcmp(ext, "bmp")) return 2;
    if (!strcmp(ext, "gif")) return 3;
    return -1;
}

static void dispimg_scan(void)
{
    s_di.count = 0;
    s_di.index = 0;
    s_di.forced_active = false;
    DIR *dir = opendir(DISPLAYIMG_PATH);
    if (!dir) {
        log_error("dispimg: open dir %s failed", DISPLAYIMG_PATH);
        return;
    }
    struct dirent *e;
    while ((e = readdir(dir)) != NULL && s_di.count < DISPLAYIMG_MAX_COUNT) {
        if (e->d_type != DT_REG) continue;
        int t = dispimg_type(e->d_name);
        if (t < 0) continue;
        snprintf(s_di.files[s_di.count].img_path, DISPLAYIMG_MAX_PATH_LENGTH,
                 "A:%s%s", DISPLAYIMG_PATH, e->d_name);
        s_di.files[s_di.count].is_gif = (t == 3);
        s_di.count++;
    }
    closedir(dir);
    log_info("dispimg: found %d valid files", s_di.count);
}

const char *ui_backend_dispimg_size(void)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", s_di.count ? s_di.index + 1 : 0, s_di.count);
    return buf;
}
bool ui_backend_dispimg_has_warning(void)
{
    return !s_di.forced_active && s_di.count == 0;
}
const char *ui_backend_dispimg_path(void)
{
    if (s_di.forced_active) return s_di.forced;
    if (s_di.count == 0) return "";
    return s_di.files[s_di.index].img_path;
}
bool ui_backend_dispimg_is_gif(void)
{
    if (s_di.forced_active) return s_di.forced_is_gif;
    if (s_di.count == 0) return false;
    return s_di.files[s_di.index].is_gif;
}
void ui_backend_displayimg_key(uint32_t key)
{
    s_di.forced_active = false; // 用户翻页即退出强制图
    if (s_di.count == 0) return;
    if (key == LV_KEY_LEFT) {
        if (--s_di.index < 0) s_di.index = 0;
    } else if (key == LV_KEY_RIGHT) {
        if (++s_di.index >= s_di.count) s_di.index = s_di.count - 1;
    }
}
// 供 ui_services_device 的 ui_displayimg_force_dispimg 调用
void ui_backend_dispimg_force(const char *path)
{
    int t = dispimg_type(path);
    if (t < 0) return;
    snprintf(s_di.forced, sizeof(s_di.forced), "A:%s", path);
    s_di.forced_is_gif = (t == 3);
    s_di.forced_active = true;
}
// 供 ui_services_device 的 ui_displayimg_rescan 调用 (IPC 素材刷新联动)。
// screen_displayimg_tick 每帧按 s_di 当前状态 diff 重绘，这里只需重扫目录。
void ui_backend_dispimg_rescan(void)
{
    dispimg_scan();
}

// ================= 生命周期 =================
void ui_backend_init(void *prts, void *apps)
{
    s_prts = (prts_t *)prts;
    s_apps = (apps_t *)apps;
    dispimg_scan();
}

// ================= 通用 =================
const char *ui_backend_version(void) { return APP_VERSION_STRING; }

// ================= 亮度 / 设置 (原 actions_settings.c) =================
int32_t ui_backend_brightness_get(void) { return g_settings.brightness; }
void ui_backend_brightness_set(int32_t v)
{
    settings_lock(&g_settings);
    g_settings.brightness = v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}

int  ui_backend_sw_mode_get(void)     { return (int)g_settings.switch_mode; }
void ui_backend_sw_mode_set(int v)
{
    settings_lock(&g_settings);
    g_settings.switch_mode = (sw_mode_t)v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}
int  ui_backend_sw_interval_get(void) { return (int)g_settings.switch_interval; }
void ui_backend_sw_interval_set(int v)
{
    settings_lock(&g_settings);
    g_settings.switch_interval = (sw_interval_t)v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}
// usb_mode 已废弃（USB 归 usb_aio_handler 的 greeter 流程），接口保留给 sim/兼容
void ui_backend_usb_reset(void)       { system("usbaioctl greeter &"); }
int  ui_backend_usb_mode_get(void)    { return (int)g_settings.usb_mode; }
void ui_backend_usb_mode_set(int v)
{
    settings_lock(&g_settings);
    g_settings.usb_mode = (usb_mode_t)v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}

bool ui_backend_lowbat_trip_get(void) { return g_settings.ctrl_word.lowbat_trip; }
void ui_backend_lowbat_trip_set(bool v)
{
    settings_lock(&g_settings);
    g_settings.ctrl_word.lowbat_trip = v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}
bool ui_backend_no_intro_get(void) { return g_settings.ctrl_word.no_intro_block; }
void ui_backend_no_intro_set(bool v)
{
    settings_lock(&g_settings);
    g_settings.ctrl_word.no_intro_block = v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}
bool ui_backend_no_overlay_get(void) { return g_settings.ctrl_word.no_overlay_block; }
void ui_backend_no_overlay_set(bool v)
{
    settings_lock(&g_settings);
    g_settings.ctrl_word.no_overlay_block = v;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
}

int  ui_backend_theme_get(void) { return g_settings.theme_id; }
void ui_backend_theme_set(int id)
{
    settings_lock(&g_settings);
    g_settings.theme_id = (uint8_t)id;
    settings_unlock(&g_settings);
    settings_update(&g_settings);
    ui_theme_apply(id);
}

// ================= 存储 / sysinfo (原 actions_sysinfo.c) =================
static uint64_t fs_avail(const char *mp)
{
    struct statvfs s;
    if (statvfs(mp, &s) != 0) return 0;
    return (uint64_t)s.f_bavail * s.f_bsize;
}
static uint64_t fs_total(const char *mp)
{
    struct statvfs s;
    if (statvfs(mp, &s) != 0) return 0;
    return (uint64_t)s.f_blocks * s.f_bsize;
}
static void fmt_size(uint64_t bytes, char *buf, size_t sz)
{
    double v = (double)bytes;
    const char *u;
    if (v >= 1024.0 * 1024 * 1024 * 1024) { v /= 1024.0 * 1024 * 1024 * 1024; u = "TB"; }
    else if (v >= 1024.0 * 1024 * 1024)   { v /= 1024.0 * 1024 * 1024;        u = "GB"; }
    else                                  { v /= 1024.0 * 1024;               u = "MB"; }
    if (v >= 100)      snprintf(buf, sz, "%.0f%s", v, u);
    else if (v >= 10)  snprintf(buf, sz, "%.1f%s", v, u);
    else               snprintf(buf, sz, "%.2f%s", v, u);
}

int32_t ui_backend_nand_percent(void)
{
    uint64_t total = fs_total(NAND_MOUNT_POINT);
    if (!total) return 0;
    return (int32_t)(((total - fs_avail(NAND_MOUNT_POINT)) * 100) / total);
}
int32_t ui_backend_sd_percent(void)
{
    if (!is_sdcard_inserted()) return 0;
    uint64_t total = fs_total(SD_MOUNT_POINT);
    if (!total) return 0;
    return (int32_t)(((total - fs_avail(SD_MOUNT_POINT)) * 100) / total);
}
const char *ui_backend_nand_label(void)
{
    static char buf[128], used[32], tot[32];
    uint64_t total = fs_total(NAND_MOUNT_POINT);
    fmt_size(total - fs_avail(NAND_MOUNT_POINT), used, sizeof(used));
    fmt_size(total, tot, sizeof(tot));
    snprintf(buf, sizeof(buf), "%s/%s", used, tot);
    return buf;
}
const char *ui_backend_sd_label(void)
{
    static char buf[128], used[32], tot[32];
    if (!is_sdcard_inserted()) return "SD卡不存在";
    if (!g_use_sd)             return "SD卡挂载失败";
    uint64_t total = fs_total(SD_MOUNT_POINT);
    fmt_size(total - fs_avail(SD_MOUNT_POINT), used, sizeof(used));
    fmt_size(total, tot, sizeof(tot));
    snprintf(buf, sizeof(buf), "%s/%s", used, tot);
    return buf;
}

static int read_head_lines(const char *path, char *ret, size_t sz, int n)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[128];
    size_t used = 0;
    int cnt = 0;
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (used + len < sz) { memcpy(ret + used, line, len); used += len; ret[used] = '\0'; }
        else break;
        for (size_t i = 0; i < len; i++) if (line[i] == '\n') { if (++cnt >= n) goto done; }
    }
done:
    fclose(fp);
    return cnt;
}

static const uint32_t crc_table[256] = {
    0x00000000L,0x77073096L,0xee0e612cL,0x990951baL,0x076dc419L,0x706af48fL,0xe963a535L,0x9e6495a3L,
    0x0edb8832L,0x79dcb8a4L,0xe0d5e91eL,0x97d2d988L,0x09b64c2bL,0x7eb17cbdL,0xe7b82d07L,0x90bf1d91L,
    0x1db71064L,0x6ab020f2L,0xf3b97148L,0x84be41deL,0x1adad47dL,0x6ddde4ebL,0xf4d4b551L,0x83d385c7L,
    0x136c9856L,0x646ba8c0L,0xfd62f97aL,0x8a65c9ecL,0x14015c4fL,0x63066cd9L,0xfa0f3d63L,0x8d080df5L,
    0x3b6e20c8L,0x4c69105eL,0xd56041e4L,0xa2677172L,0x3c03e4d1L,0x4b04d447L,0xd20d85fdL,0xa50ab56bL,
    0x35b5a8faL,0x42b2986cL,0xdbbbc9d6L,0xacbcf940L,0x32d86ce3L,0x45df5c75L,0xdcd60dcfL,0xabd13d59L,
    0x26d930acL,0x51de003aL,0xc8d75180L,0xbfd06116L,0x21b4f4b5L,0x56b3c423L,0xcfba9599L,0xb8bda50fL,
    0x2802b89eL,0x5f058808L,0xc60cd9b2L,0xb10be924L,0x2f6f7c87L,0x58684c11L,0xc1611dabL,0xb6662d3dL,
    0x76dc4190L,0x01db7106L,0x98d220bcL,0xefd5102aL,0x71b18589L,0x06b6b51fL,0x9fbfe4a5L,0xe8b8d433L,
    0x7807c9a2L,0x0f00f934L,0x9609a88eL,0xe10e9818L,0x7f6a0dbbL,0x086d3d2dL,0x91646c97L,0xe6635c01L,
    0x6b6b51f4L,0x1c6c6162L,0x856530d8L,0xf262004eL,0x6c0695edL,0x1b01a57bL,0x8208f4c1L,0xf50fc457L,
    0x65b0d9c6L,0x12b7e950L,0x8bbeb8eaL,0xfcb9887cL,0x62dd1ddfL,0x15da2d49L,0x8cd37cf3L,0xfbd44c65L,
    0x4db26158L,0x3ab551ceL,0xa3bc0074L,0xd4bb30e2L,0x4adfa541L,0x3dd895d7L,0xa4d1c46dL,0xd3d6f4fbL,
    0x4369e96aL,0x346ed9fcL,0xad678846L,0xda60b8d0L,0x44042d73L,0x33031de5L,0xaa0a4c5fL,0xdd0d7cc9L,
    0x5005713cL,0x270241aaL,0xbe0b1010L,0xc90c2086L,0x5768b525L,0x206f85b3L,0xb966d409L,0xce61e49fL,
    0x5edef90eL,0x29d9c998L,0xb0d09822L,0xc7d7a8b4L,0x59b33d17L,0x2eb40d81L,0xb7bd5c3bL,0xc0ba6cadL,
    0xedb88320L,0x9abfb3b6L,0x03b6e20cL,0x74b1d29aL,0xead54739L,0x9dd277afL,0x04db2615L,0x73dc1683L,
    0xe3630b12L,0x94643b84L,0x0d6d6a3eL,0x7a6a5aa8L,0xe40ecf0bL,0x9309ff9dL,0x0a00ae27L,0x7d079eb1L,
    0xf00f9344L,0x8708a3d2L,0x1e01f268L,0x6906c2feL,0xf762575dL,0x806567cbL,0x196c3671L,0x6e6b06e7L,
    0xfed41b76L,0x89d32be0L,0x10da7a5aL,0x67dd4accL,0xf9b9df6fL,0x8ebeeff9L,0x17b7be43L,0x60b08ed5L,
    0xd6d6a3e8L,0xa1d1937eL,0x38d8c2c4L,0x4fdff252L,0xd1bb67f1L,0xa6bc5767L,0x3fb506ddL,0x48b2364bL,
    0xd80d2bdaL,0xaf0a1b4cL,0x36034af6L,0x41047a60L,0xdf60efc3L,0xa867df55L,0x316e8eefL,0x4669be79L,
    0xcb61b38cL,0xbc66831aL,0x256fd2a0L,0x5268e236L,0xcc0c7795L,0xbb0b4703L,0x220216b9L,0x5505262fL,
    0xc5ba3bbeL,0xb2bd0b28L,0x2bb45a92L,0x5cb36a04L,0xc2d7ffa7L,0xb5d0cf31L,0x2cd99e8bL,0x5bdeae1dL,
    0x9b64c2b0L,0xec63f226L,0x756aa39cL,0x026d930aL,0x9c0906a9L,0xeb0e363fL,0x72076785L,0x05005713L,
    0x95bf4a82L,0xe2b87a14L,0x7bb12baeL,0x0cb61b38L,0x92d28e9bL,0xe5d5be0dL,0x7cdcefb7L,0x0bdbdf21L,
    0x86d3d2d4L,0xf1d4e242L,0x68ddb3f8L,0x1fda836eL,0x81be16cdL,0xf6b9265bL,0x6fb077e1L,0x18b74777L,
    0x88085ae6L,0xff0f6a70L,0x66063bcaL,0x11010b5cL,0x8f659effL,0xf862ae69L,0x616bffd3L,0x166ccf45L,
    0xa00ae278L,0xd70dd2eeL,0x4e048354L,0x3903b3c2L,0xa7672661L,0xd06016f7L,0x4969474dL,0x3e6e77dbL,
    0xaed16a4aL,0xd9d65adcL,0x40df0b66L,0x37d83bf0L,0xa9bcae53L,0xdebb9ec5L,0x47b2cf7fL,0x30b5ffe9L,
    0xbdbdf21cL,0xcabac28aL,0x53b39330L,0x24b4a3a6L,0xbad03605L,0xcdd70693L,0x54de5729L,0x23d967bfL,
    0xb3667a2eL,0xc4614ab8L,0x5d681b02L,0x2a6f2b94L,0xb40bbe37L,0xc30c8ea1L,0x5a05df1bL,0x2d02ef8dL
};
static uint32_t crc32_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    unsigned char buf[1024];
    size_t n;
    uint32_t crc = 0xffffffffUL;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        for (size_t i = 0; i < n; i++) crc = crc_table[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
    fclose(fp);
    return crc ^ 0xffffffffUL;
}

const char *ui_backend_sysinfo_text(void)
{
    static char buf[2048];
    static int call_cnt = 0;
    if (call_cnt != 0) {            // 每 300 次刷新一次 (约 10s, tick 频率)
        if (++call_cnt >= 300) call_cnt = 0;
        return buf;
    }
    call_cnt = 1;
    char mem[512] = {0}, os[512] = {0};
    read_head_lines("/proc/meminfo", mem, sizeof(mem), 3);
    read_head_lines("/etc/os-release", os, sizeof(os), 2);
    snprintf(buf, sizeof(buf),
             "罗德岛电子通行认证程序-代号:%s\n"
             "版本号: %s 校验码: %08X\n"
             "程序生成时间: %s\n"
             "%s%s%s",
             APP_SUBCODENAME, APP_VERSION_STRING, crc32_file("/root/epass_drm_app"),
             COMPILE_TIME, mem, os, APP_ABOUT_MSG);
    return buf;
}

// ================= 干员列表 (原 actions_oplist.c) =================
int ui_backend_oplist_count(void) { return s_prts ? s_prts->operator_count : 0; }
static const char *disp_type_badge(display_type_t t)
{
    switch (t) {
        case DISPLAY_360_640:  return "360";
        case DISPLAY_480_854:  return "480";
        case DISPLAY_720_1280: return "720";
        default:               return NULL;
    }
}
bool ui_backend_oplist_get(int idx, ui_op_entry_t *out)
{
    if (!s_prts || idx < 0 || idx >= s_prts->operator_count) return false;
    prts_operator_entry_t *op = &s_prts->operators[idx];
    out->name      = op->operator_name;
    out->desc      = op->description;
    out->logo_path = op->icon_path;
    out->sd        = (op->source != PRTS_SOURCE_NAND);
    out->res       = disp_type_badge(op->disp_type);
    return true;
}
int ui_backend_oplist_current(void) { return s_prts ? s_prts->operator_index : 0; }
void ui_backend_oplist_select(int idx)
{
    if (s_prts) prts_request_set_operator(s_prts, idx);
}
void ui_backend_oplist_refresh(void)
{
    if (s_prts) prts_request_reload_assets(s_prts);
}

// ================= 应用列表 (原 actions_apps.c) =================
int ui_backend_applist_count(void) { return s_apps ? s_apps->app_count : 0; }
bool ui_backend_applist_get(int idx, ui_app_entry_t *out)
{
    if (!s_apps || idx < 0 || idx >= s_apps->app_count) return false;
    app_entry_t *a = &s_apps->apps[idx];
    out->name      = a->app_name;
    out->desc      = a->description;
    out->logo_path = a->icon_path;
    out->sd        = (a->source != APP_SOURCE_NAND);
    if (a->type == APP_TYPE_BACKGROUND) out->state = (a->pid != -1) ? UI_APP_BG : UI_APP_STOPPED;
    else                                out->state = UI_APP_FG;
    return true;
}
void ui_backend_applist_select(int idx)
{
    if (!s_apps || idx < 0 || idx >= s_apps->app_count) return;
    app_entry_t *a = &s_apps->apps[idx];
    switch (a->type) {
        case APP_TYPE_FOREGROUND_EXTENSION_ONLY:
            ui_warning(UI_WARNING_APP_NO_DIRECT_START);
            break;
        case APP_TYPE_FOREGROUND:
            apps_try_launch_by_index(s_apps, idx);
            break;
        case APP_TYPE_BACKGROUND:
            apps_toggle_bg_app_by_index(s_apps, idx);
            break;
        default:
            log_error("applist_select: unknown app type %d", a->type);
            break;
    }
}
