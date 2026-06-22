#include "screen_mainmenu.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"
#include "ui/font_registry.h"
#include "utils/log.h"
#include "icons.h"

// 按钮图标实际像素 (360 基准, 随 S() 缩放, FreeType 矢量栅格化)
#define PX_BTN_ICON 40

// 本屏私有状态：只存"之后还要访问"的少数控件。
static struct {
    lv_obj_t *brightness;
    lv_obj_t *version;
    bool      suppress_evt;
} self;

// ---- 事件回调 (即原来的 actions，就地 static) ----
static void on_oplist(lv_event_t *e)   { (void)e; screen_show(SCREEN_OPLIST); }
static void on_dispimg(lv_event_t *e)  { (void)e; screen_show(SCREEN_DISPLAYIMG); }
static void on_apps(lv_event_t *e)     { (void)e; screen_show(SCREEN_APPLIST); }
static void on_files(lv_event_t *e)    { (void)e; screen_show(SCREEN_FILEMANAGER); }
static void on_settings(lv_event_t *e) { (void)e; screen_show(SCREEN_SETTINGS); }
static void on_sysinfo(lv_event_t *e)  { (void)e; screen_show(SCREEN_SYSINFO); }
static void on_restart(lv_event_t *e)  { (void)e; log_info("[mainmenu] restart app"); }
static void on_shutdown(lv_event_t *e) { (void)e; log_info("[mainmenu] shutdown"); }

static void on_brightness(lv_event_t *e)
{
    if (self.suppress_evt) return;
    ui_backend_brightness_set(lv_slider_get_value(lv_event_get_target(e)));
}

// ---- 六宫格单格：图标(顶部居中) + 文字(底部居中) ----
static void grid_btn(lv_obj_t *parent, int x, int y,
                     const char *icon, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(parent);
    lv_obj_set_pos(o, S(x), S(y));
    lv_obj_set_size(o, S(95), S(110));
    lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);
    add_style_main_btn(o);

    lv_obj_t *ic = lv_label_create(o);
    lv_obj_set_style_text_font(ic, font_get(FONT_ICON, PX_BTN_ICON), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(ic, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(ic, S(10));
    lv_label_set_text(ic, icon);

    lv_obj_t *lbl = lv_label_create(o);
    add_style_label_large(lbl);
    lv_obj_set_style_align(lbl, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(lbl, S(-10));
    lv_label_set_text(lbl, text);
}

lv_obj_t *screen_mainmenu_create(void)
{
    memset(&self, 0, sizeof(self));

    lv_obj_t *root = ui_screen_root();
    ui_header(root, "主菜单");

    // 六宫格 (列 25/130/235，行 50/170，按钮 95x110，间距 10)
    grid_btn(root,  25,  50, UI_ICON_USER,          "干员",   on_oplist);
    grid_btn(root, 130,  50, UI_ICON_IMAGES,        "扩列图", on_dispimg);
    grid_btn(root, 235,  50, UI_ICON_BOX_ARCHIVE,   "应用",   on_apps);
    grid_btn(root,  25, 170, UI_ICON_FILE,          "文件",   on_files);
    grid_btn(root, 130, 170, UI_ICON_GEAR,          "设置",   on_settings);
    grid_btn(root, 235, 170, UI_ICON_MOBILE_SCREEN, "设备",   on_sysinfo);

    // 亮度：图标 + 滑条
    {
        lv_obj_t *ic = lv_label_create(root);
        lv_obj_set_style_text_font(ic, font_get(FONT_ICON, 28), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(ic, S(25), S(290));
        lv_label_set_text(ic, UI_ICON_SUN);
    }
    self.brightness = lv_slider_create(root);
    lv_obj_set_pos(self.brightness, S(60), S(300));
    lv_obj_set_size(self.brightness, S(270), S(10));
    lv_slider_set_range(self.brightness, 1, 9);
    lv_obj_add_event_cb(self.brightness, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    // 重启 / 关机 (各 150x50，左右 margin 25 对称，间距 10)
    {
        lv_obj_t *b = ui_text_button(root, 25, 325, 150, 50, 0, "重启程序", on_restart);
        add_style_main_small_btn(b);
        lv_obj_t *b2 = ui_text_button(root, 185, 325, 150, 50, 0, "关机", on_shutdown);
        add_style_main_small_btn(b2);
    }

    // 版本号 (tick 更新，接在版权第一行后) + 版权信息
    self.version = lv_label_create(root);
    lv_obj_set_pos(self.version, S(160), S(385));
    add_style_label_small(self.version);
    lv_label_set_text(self.version, "");
    {
        lv_obj_t *o = lv_label_create(root);
        lv_obj_set_pos(o, S(25), S(385));
        add_style_label_small(o);
        lv_label_set_text(o,
            "电子通行证播放程序\n罗德岛工程部 白银 <inapp@iccmc.cc> Et al.2026 \n"
            "本项目是开源的自由硬件.不带任何形式的保证.\n作者不获取任何利润 github.com/rhodesepass");
    }

    return root;
}

void screen_mainmenu_tick(void)
{
    const char *v = ui_backend_version();
    if (strcmp(v, lv_label_get_text(self.version)) != 0) {
        lv_label_set_text(self.version, v);
    }
    int32_t b = ui_backend_brightness_get();
    if (b != lv_slider_get_value(self.brightness)) {
        self.suppress_evt = true;
        lv_slider_set_value(self.brightness, b, LV_ANIM_ON);
        self.suppress_evt = false;
    }
}
