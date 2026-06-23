#include "screen_settings.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"
#include "ui/font_registry.h"
#include "utils/log.h"

static struct {
    lv_obj_t *sw_mode, *sw_int, *usb;   // 下拉
} self;

// ---- 开关回调 ----
static void on_lowbat(lv_event_t *e)    { ui_backend_lowbat_trip_set(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)); }
static void on_no_intro(lv_event_t *e)  { ui_backend_no_intro_set(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)); }
static void on_no_overlay(lv_event_t *e){ ui_backend_no_overlay_set(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)); }
// ---- 下拉回调 ----
static void on_sw_mode(lv_event_t *e)   { ui_backend_sw_mode_set(lv_dropdown_get_selected(lv_event_get_target(e))); }
static void on_sw_int(lv_event_t *e)    { ui_backend_sw_interval_set(lv_dropdown_get_selected(lv_event_get_target(e))); }
static void on_usb(lv_event_t *e)       { ui_backend_usb_mode_set(lv_dropdown_get_selected(lv_event_get_target(e))); }
// ---- 按钮 ----
static void on_clear_cache(lv_event_t *e){ (void)e; log_info("[settings] clear cache"); }
static void on_srgn(lv_event_t *e)       { (void)e; log_info("[settings] enter srgn_config"); }
static void on_back(lv_event_t *e)       { (void)e; screen_show(SCREEN_MAINMENU); }

static lv_obj_t *make_switch(lv_obj_t *root, int y, const char *text, bool on, lv_event_cb_t cb)
{
    lv_obj_t *lbl = lv_label_create(root);
    lv_obj_set_pos(lbl, S(22), S(y)); add_style_label_large(lbl); lv_label_set_text(lbl, text);
    lv_obj_t *sw = lv_switch_create(root);
    lv_obj_set_pos(sw, S(279), S(y)); lv_obj_set_size(sw, S(60), S(29));
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

static lv_obj_t *make_dropdown(lv_obj_t *root, int x, int y, int w, int lbl_y,
                               const char *title, const char *opts, int sel, lv_event_cb_t cb)
{
    lv_obj_t *t = lv_label_create(root);
    lv_obj_set_pos(t, S(x), S(lbl_y)); add_style_label_large(t); lv_label_set_text(t, title);
    lv_obj_t *dd = lv_dropdown_create(root);
    lv_obj_set_pos(dd, S(x), S(y)); lv_obj_set_width(dd, S(w));
    lv_dropdown_set_options(dd, opts);
    lv_dropdown_set_selected(dd, sel);
    lv_obj_set_style_text_font(dd, font_get(FONT_BODY, 14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return dd;
}

lv_obj_t *screen_settings_create(void)
{
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "设备参数定值");

    ui_small_text_button(root, 205, 8, 82, 32, 0, "清除缓存", on_clear_cache);

    make_switch(root, 49,  "低电量自动关机",        ui_backend_lowbat_trip_get(),  on_lowbat);
    make_switch(root, 93,  "(切换时)跳过入场动画",   ui_backend_no_intro_get(),     on_no_intro);
    make_switch(root, 136, "(切换时)不显示信息层",   ui_backend_no_overlay_get(),   on_no_overlay);

    self.sw_mode = make_dropdown(root, 23, 211, 151, 175, "切换模式",
                                 "顺序播放\n随机播放\n手动切换", ui_backend_sw_mode_get(), on_sw_mode);
    self.sw_int  = make_dropdown(root, 195, 210, 144, 175, "自动切换间隔",
                                 "1分钟\n3分钟\n5分钟\n10分钟\n30分钟", ui_backend_sw_interval_get(), on_sw_int);
    self.usb     = make_dropdown(root, 22, 292, 151, 263, "USB模式",
                                 "文件(MTP)\nShell(串口)\n网络(rndis)\n仅充电\n管理器APP", ui_backend_usb_mode_get(), on_usb);

    ui_text_button(root, 23, 513, 316, 52, 0xff8c0f0f, "进入底层设置srgn_config", on_srgn);
    ui_text_button(root, 23, 574, 316, 51, 0, "返回", on_back);

    return root;
}

// 下拉外部变更时同步 (编辑中不打扰)
static void sync_dd(lv_obj_t *dd, int v)
{
    if (!(lv_obj_get_state(dd) & LV_STATE_EDITED) && lv_dropdown_get_selected(dd) != (uint32_t)v) {
        lv_dropdown_set_selected(dd, v);
    }
}
void screen_settings_tick(void)
{
    sync_dd(self.sw_mode, ui_backend_sw_mode_get());
    sync_dd(self.sw_int,  ui_backend_sw_interval_get());
    sync_dd(self.usb,     ui_backend_usb_mode_get());
}
