//
// sim 后端桩 —— 实现 ui_backend.h，喂占位数据让手写屏脱离设备子系统在桌面跑。
//
#include "ui_screens/ui_backend.h"
#include "ui/ui_theme.h"
#include "utils/log.h"

#define LOGO UI_IMG_DIR "/prts_64_inv.png"

// ---- 生命周期 ----
void ui_backend_init(void *prts, void *apps) { (void)prts; (void)apps; }

// ---- 通用 / 亮度 ----
static int32_t s_brightness = 5;
const char *ui_backend_version(void) { return "SIM-skeleton"; }
int32_t ui_backend_brightness_get(void) { return s_brightness; }
void ui_backend_brightness_set(int32_t v) { s_brightness = v; log_info("[mock] brightness=%d", v); }

// ---- 设备信息 / 存储 ----
int32_t ui_backend_nand_percent(void) { return 62; }
int32_t ui_backend_sd_percent(void)   { return 35; }
const char *ui_backend_nand_label(void) { return "5.0 / 8.0 GiB"; }
const char *ui_backend_sd_label(void)   { return "11 / 32 GiB"; }
const char *ui_backend_sysinfo_text(void)
{
    return "EPass DRM System (SIM)\n"
           "Board   : T113-s3 / F1C200s\n"
           "Kernel  : 5.4.x\n"
           "Uptime  : 03:14:15\n"
           "MemFree : 28.3 MiB\n"
           "Operators: 3  Apps: 2";
}

// ---- 扩列图 ----
const char *ui_backend_dispimg_size(void)    { return "1/1"; }
bool        ui_backend_dispimg_has_warning(void) { return true; } // sim 无图，显示提示
const char *ui_backend_dispimg_path(void)    { return ""; }
bool        ui_backend_dispimg_is_gif(void)  { return false; }
void        ui_backend_displayimg_key(uint32_t key) { (void)key; }

// ---- 设置 ----
static int s_sw_mode = 0, s_sw_interval = 1, s_usb_mode = 0;
static bool s_lowbat = true, s_no_intro = false, s_no_overlay = false;
int  ui_backend_sw_mode_get(void) { return s_sw_mode; }
void ui_backend_sw_mode_set(int v) { s_sw_mode = v; log_info("[mock] sw_mode=%d", v); }
int  ui_backend_sw_interval_get(void) { return s_sw_interval; }
void ui_backend_sw_interval_set(int v) { s_sw_interval = v; log_info("[mock] sw_interval=%d", v); }
int  ui_backend_usb_mode_get(void) { return s_usb_mode; }
void ui_backend_usb_mode_set(int v) { s_usb_mode = v; log_info("[mock] usb_mode=%d", v); }
bool ui_backend_lowbat_trip_get(void) { return s_lowbat; }
void ui_backend_lowbat_trip_set(bool v) { s_lowbat = v; log_info("[mock] lowbat=%d", v); }
bool ui_backend_no_intro_get(void) { return s_no_intro; }
void ui_backend_no_intro_set(bool v) { s_no_intro = v; log_info("[mock] no_intro=%d", v); }
bool ui_backend_no_overlay_get(void) { return s_no_overlay; }
void ui_backend_no_overlay_set(bool v) { s_no_overlay = v; log_info("[mock] no_overlay=%d", v); }
static int s_theme = 0;
int  ui_backend_theme_get(void) { return s_theme; }
void ui_backend_theme_set(int id) { s_theme = id; log_info("[mock] theme=%d (%s)", id, ui_theme_name(id)); ui_theme_apply(id); }

// ---- 干员列表 ----
static const ui_op_entry_t s_ops[] = {
    { "新约能天使", "谁不喜欢能天使呢？\n素材作者: 白银。", LOGO, false },
    { "假日威龙陈", "节日限定皮肤。\n素材作者: 伊卡洛斯sama。", LOGO, true },
    { "标准型号-W", "玩归玩闹归闹。\n素材作者: 薄云。", LOGO, false },
};
int ui_backend_oplist_count(void) { return (int)(sizeof(s_ops) / sizeof(s_ops[0])); }
bool ui_backend_oplist_get(int idx, ui_op_entry_t *out)
{
    if (idx < 0 || idx >= ui_backend_oplist_count()) return false;
    *out = s_ops[idx];
    return true;
}
int  ui_backend_oplist_current(void) { return 0; }
void ui_backend_oplist_select(int idx) { log_info("[mock] op select %d", idx); }
void ui_backend_oplist_refresh(void)   { log_info("[mock] op refresh"); }

// ---- 应用列表 ----
static const ui_app_entry_t s_apps[] = {
    { "测试应用 2233", "白银白银我们喜欢你！\n作者: 伊卡洛斯sama。", LOGO, UI_APP_BG, false },
    { "调试器 DbgTool", "底层调试工具。\n作者: 薄云。", LOGO, UI_APP_STOPPED, true },
};
int ui_backend_applist_count(void) { return (int)(sizeof(s_apps) / sizeof(s_apps[0])); }
bool ui_backend_applist_get(int idx, ui_app_entry_t *out)
{
    if (idx < 0 || idx >= ui_backend_applist_count()) return false;
    *out = s_apps[idx];
    return true;
}
void ui_backend_applist_select(int idx) { log_info("[mock] app select %d", idx); }
