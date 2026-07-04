#pragma once
//
// ui_backend —— 手写 UI 的精简后端 seam (取代 EEZ 的 vars 那一坨)。
//
// UI 与"数据/业务"之间唯一的注入点：
//   - 设备侧实现接 settings / prts / apps / 电量 等真实子系统；
//   - PC 模拟器由 sim/mock_backend.c 提供桩 (占位数据)。
// 各屏按需 pull，不再走全局 setter + tick 轮询那套强制机制。
//
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- 生命周期 ----
// 设备侧注入 prts/apps 句柄 (sim 忽略)。须在建屏前调。指针用 void* 以免本头依赖 prts/apps。
void ui_backend_init(void *prts, void *apps);

// ---- 通用 ----
const char *ui_backend_version(void);

// ---- 亮度 (1..9) ----
int32_t ui_backend_brightness_get(void);
void    ui_backend_brightness_set(int32_t value);

// ---- 设备信息 / 存储 ----
int32_t     ui_backend_nand_percent(void);
int32_t     ui_backend_sd_percent(void);
const char *ui_backend_nand_label(void);
const char *ui_backend_sd_label(void);
const char *ui_backend_sysinfo_text(void);

// ---- 扩列图 ----
const char *ui_backend_dispimg_size(void);
bool        ui_backend_dispimg_has_warning(void); // true=无图，显示提示
const char *ui_backend_dispimg_path(void);        // 当前图 lv_fs 路径 (含 "A:" 盘符)
bool        ui_backend_dispimg_is_gif(void);       // 当前图是否 GIF

// ---- 设置 (下拉为选中索引) ----
int  ui_backend_sw_mode_get(void);      void ui_backend_sw_mode_set(int v);
int  ui_backend_sw_interval_get(void);  void ui_backend_sw_interval_set(int v);
int  ui_backend_usb_mode_get(void);     void ui_backend_usb_mode_set(int v);
bool ui_backend_lowbat_trip_get(void);  void ui_backend_lowbat_trip_set(bool v);
bool ui_backend_no_intro_get(void);     void ui_backend_no_intro_set(bool v);
bool ui_backend_no_overlay_get(void);   void ui_backend_no_overlay_set(bool v);
// 配色方案索引 (见 ui_theme 预设)。set 会持久化并立即应用 (ui_theme_apply)。
int  ui_backend_theme_get(void);        void ui_backend_theme_set(int id);

// ---- 干员列表 ----
typedef struct {
    const char *name;
    const char *desc;
    const char *logo_path; // lv_fs 路径，NULL 用占位
    bool        sd;        // 来自 SD 卡
} ui_op_entry_t;
int  ui_backend_oplist_count(void);
bool ui_backend_oplist_get(int idx, ui_op_entry_t *out);
// 当前选中干员索引 (进屏时聚焦用)；无则 0。
int  ui_backend_oplist_current(void);
// 选中干员 (设备=prts_request_set_operator)。调用方随后切到 spinner。
void ui_backend_oplist_select(int idx);
// 重新加载干员素材 (设备=prts_request_reload_assets)。调用方随后切到 spinner。
void ui_backend_oplist_refresh(void);

// ---- 应用列表 ----
typedef enum { UI_APP_FG, UI_APP_BG, UI_APP_STOPPED } ui_app_state_t;
typedef struct {
    const char    *name;
    const char    *desc;
    const char    *logo_path;
    ui_app_state_t state;
    bool           sd;
} ui_app_entry_t;
int  ui_backend_applist_count(void);
bool ui_backend_applist_get(int idx, ui_app_entry_t *out);
// 选中应用 (设备=按 type 启动/切后台/告警)。调用方随后切到 spinner。
void ui_backend_applist_select(int idx);

// ---- 扩列图按键导航 (←/→ 翻页) ----
void ui_backend_displayimg_key(uint32_t key);

#ifdef __cplusplus
}
#endif
