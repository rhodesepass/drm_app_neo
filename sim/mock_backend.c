//
// sim 后端桩 —— 实现 ui_backend.h，喂占位数据让手写屏脱离设备子系统在桌面跑。
// 数据仿照真机观感编造 (截图/演示不穿帮)；可选环境变量注入真实素材:
//   SIM_DISPIMG=<png绝对路径>  扩列图屏显示该图 (不设则显示"无图"提示)
//   SIM_APPS_DIR=<epass_applications/applications 绝对路径>  应用列表用真实图标
//
#include "ui_screens/ui_backend.h"
#include "ui/ui_theme.h"
#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>

#define LOGO UI_IMG_DIR "/prts_64_inv.png"

// ---- 生命周期 ----
void ui_backend_init(void *prts, void *apps) { (void)prts; (void)apps; }

// ---- 通用 / 亮度 ----
static int32_t s_brightness = 5;
const char *ui_backend_version(void) { return "a2.7.0_7171727"; }
int32_t ui_backend_brightness_get(void) { return s_brightness; }
void ui_backend_brightness_set(int32_t v) { s_brightness = v; log_info("[mock] brightness=%d", v); }

// ---- 设备信息 / 存储 ----
int32_t ui_backend_nand_percent(void) { return 82; }
int32_t ui_backend_sd_percent(void)   { return 11; }
const char *ui_backend_nand_label(void) { return "98 / 119 MiB"; }
const char *ui_backend_sd_label(void)   { return "3.2 / 29.7 GiB"; }
const char *ui_backend_sysinfo_text(void)
{
    // 与 ui_backend_device.c 同一版式: 代号/版本/校验 + meminfo 3行 + os-release 2行 + 关于
    return "罗德岛电子通行认证程序-代号:proj0cpy\n"
           "版本号: a2.7.0_7171727 校验码: 8C3F51A9\n"
           "程序生成时间: 2026-07-10 21:34:56\n"
           "MemTotal:          55620 kB\n"
           "MemFree:           28932 kB\n"
           "MemAvailable:      41208 kB\n"
           "NAME=Buildroot\n"
           "VERSION=2024.02\n"
           "基于LVGL和寄存器魔法的方舟通行证展示程序\n"
           "电子通行证 Contributers 2026 GPLV3\n"
           "白银 伊卡洛斯sama 薄云 Et al.\n"
           "https://github.com/rhodesepass\n";
}

// ---- 扩列图 ----
static const char *dispimg_env(void) { return getenv("SIM_DISPIMG"); }
const char *ui_backend_dispimg_size(void) { return dispimg_env() ? "2/5" : "1/1"; }
bool        ui_backend_dispimg_has_warning(void) { return dispimg_env() == NULL; }
const char *ui_backend_dispimg_path(void)
{
    static char path[512];
    const char *p = dispimg_env();
    if (!p) return "";
    snprintf(path, sizeof(path), "A:%s", p);  // lv_fs 盘符
    return path;
}
bool ui_backend_dispimg_is_gif(void)  { return false; }
void ui_backend_displayimg_key(uint32_t key) { (void)key; }

// ---- 设置 ----
static int s_sw_mode = 0, s_sw_interval = 1, s_usb_mode = 0;
static bool s_lowbat = true, s_no_intro = false, s_no_overlay = false;
int  ui_backend_sw_mode_get(void) { return s_sw_mode; }
void ui_backend_sw_mode_set(int v) { s_sw_mode = v; log_info("[mock] sw_mode=%d", v); }
int  ui_backend_sw_interval_get(void) { return s_sw_interval; }
void ui_backend_sw_interval_set(int v) { s_sw_interval = v; log_info("[mock] sw_interval=%d", v); }
int  ui_backend_usb_mode_get(void) { return s_usb_mode; }
void ui_backend_usb_mode_set(int v) { s_usb_mode = v; log_info("[mock] usb_mode=%d", v); }
void ui_backend_usb_reset(void) { log_info("[mock] usb reset -> greeter"); }
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
    { "新约能天使", "谁不喜欢能天使呢？\n素材作者: 白银。", LOGO, false, "360" },
    { "假日威龙陈", "节日限定皮肤。\n素材作者: 伊卡洛斯sama。", LOGO, false, "720" },
    { "标准型号-W", "玩归玩闹归闹。\n素材作者: 薄云。", LOGO, false, "360" },
    { "缄默德克萨斯", "企鹅物流资深干员。\n素材作者: 白银。", LOGO, true, "720" },
    { "焰影苇草", "重塑之手。\n素材作者: 伊卡洛斯sama。", LOGO, true, "360" },
    { "琳琅诗怀雅", "岁相·诗怀雅。\n素材作者: 薄云。", LOGO, true, "720" },
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
// 名称/描述抄 epass_applications 各 appconfig.json; 设 SIM_APPS_DIR 时用真实图标。
typedef struct { const char *folder, *name, *desc; ui_app_state_t state; bool sd; } mock_app_t;
static const mock_app_t s_apps[] = {
    { "ebook_reader",  "电子书阅读器", "支持 UTF-8 与 GBK 编码 TXT 的四键电子书阅读器。", UI_APP_STOPPED, false },
    { "image_viewer",  "图片查看器", "支持 JPG/PNG/BMP/GIF 的四键图片查看器，可在同目录内前后翻页。", UI_APP_STOPPED, false },
    { "quick_start",   "快速上手", "新手教程：按键布局、设备自检与主程序各屏用法。", UI_APP_STOPPED, false },
    { "system_maintenance", "系统维护", "查看启动配置、管理设备维护操作并安全格式化 SD 卡。", UI_APP_STOPPED, false },
    { "tetris",        "俄罗斯方块", "经典俄罗斯方块，支持移动、旋转、软降、消行和计分。", UI_APP_STOPPED, true },
    { "snake",         "贪吃蛇", "四键控制的经典贪吃蛇游戏。", UI_APP_STOPPED, true },
    { "game_2048",     "2048", "适配四键操作的经典 2048 数字合并游戏。", UI_APP_STOPPED, true },
};
int ui_backend_applist_count(void) { return (int)(sizeof(s_apps) / sizeof(s_apps[0])); }
bool ui_backend_applist_get(int idx, ui_app_entry_t *out)
{
    if (idx < 0 || idx >= ui_backend_applist_count()) return false;
    static char icons[sizeof(s_apps) / sizeof(s_apps[0])][512];
    const char *apps_dir = getenv("SIM_APPS_DIR");
    const mock_app_t *a = &s_apps[idx];
    out->name = a->name;
    out->desc = a->desc;
    out->state = a->state;
    out->sd = a->sd;
    if (apps_dir) {
        snprintf(icons[idx], sizeof(icons[idx]), "A:%s/%s/icon.png", apps_dir, a->folder);
        out->logo_path = icons[idx];
    } else {
        out->logo_path = LOGO;
    }
    return true;
}
void ui_backend_applist_select(int idx) { log_info("[mock] app select %d", idx); }
