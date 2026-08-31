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

#define PX_BTN_ICON 55

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
static void on_restart(lv_event_t *e)  { (void)e; ui_hook_restart(); }
static void on_shutdown(lv_event_t *e) { (void)e; ui_hook_shutdown_request(); }

static void on_brightness(lv_event_t *e)
{
    if (self.suppress_evt) return;
    ui_backend_brightness_set(lv_slider_get_value(lv_event_get_target(e)));
}

// 设备上按键 1 在上、2 在下，但 1->LV_KEY_LEFT(减)、2->LV_KEY_RIGHT(加)，
// 用起来反手。这里在 slider 处理前拦截并翻转方向：上键(1)加、下键(2)减。
// slider 自带的 LV_EVENT_KEY 处理不理会 bar 的 reversed 标志，所以只能自己接管。
static void on_brightness_key(lv_event_t *e)
{
    lv_obj_t *s   = lv_event_get_target(e);
    uint32_t  key = lv_event_get_key(e);
    int32_t   v   = lv_slider_get_value(s);

    if (key == LV_KEY_LEFT || key == LV_KEY_DOWN)       v += 1;
    else if (key == LV_KEY_RIGHT || key == LV_KEY_UP)   v -= 1;
    else return;

    lv_slider_set_value(s, v, LV_ANIM_ON);
    lv_obj_send_event(s, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_stop_processing(e); // 阻止 slider 默认(未翻转)的按键处理
}

// 宫格内容布局兜底 (default 类表未挂 grid_layout 时用)
static const ui_grid_layout_t grid_layout_default = {
    .icon_align = LV_ALIGN_CENTER, .icon_x = 0, .icon_y = 5, .icon_color = UI_C_COUNT,
    .text_align = LV_ALIGN_CENTER, .text_x = 0, .text_y = 5, .text_color = UI_C_COUNT,
};

static void grid_btn(lv_obj_t *parent, int x, int y, ui_of_slot_t slot,
                     ui_menu_icon_t icon, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(parent);
    lv_obj_set_size(o, S(95), S(110));
    lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);
    add_class(o, UI_CLS_MAIN_GRID);
    ui_place(o, x, y, slot);   // 位置/尺寸可由主题按槽位微调

    const ui_grid_layout_t *lo = ui_theme_class(UI_CLS_MAIN_GRID)->grid_layout;
    if (!lo) lo = &grid_layout_default;

    lv_obj_t *ic = lv_label_create(o);
    lv_obj_set_style_text_font(ic, font_get(FONT_ICON, PX_BTN_ICON), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (lo->icon_w > 0) {
        lv_obj_set_width(ic, S(lo->icon_w));
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (lo->icon_h > 0)
        lv_obj_set_height(ic, S(lo->icon_h));
    lv_obj_set_style_align(ic, lo->icon_align, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(ic, S(lo->icon_x), S(lo->icon_y));
    if (lo->icon_color != UI_C_COUNT)
        lv_obj_set_style_text_color(ic, ui_color(lo->icon_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ic, ui_theme_menu_icon(icon));

    lv_obj_t *lbl = lv_label_create(o);
    add_style_label_large(lbl);
    lv_obj_set_style_align(lbl, lo->text_align, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl, S(lo->text_x), S(lo->text_y));
    if (lo->text_color != UI_C_COUNT)
        lv_obj_set_style_text_color(lbl, ui_color(lo->text_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(lbl, text);
}

lv_obj_t *screen_mainmenu_create(void)
{
    memset(&self, 0, sizeof(self));

    lv_obj_t *root = ui_screen_root();
    // 页面装饰 (顶栏/风格条/底部装饰条) 由当前主题的 decorate 钩子构建 (见 ui_theme_decorate)。
    ui_theme_decorate(root, ui_theme_title(SCREEN_MAINMENU, "主菜单"));

    // 六宫格 (列 25/130/235，行 50/170，按钮 95x110，间距 10；位置/图标随主题)
    grid_btn(root,  25,  50, UI_OF_SLOT_MAIN_GRID_OPLIST,   UI_MENU_ICON_OPLIST,   "干员",   on_oplist);
    grid_btn(root, 130,  50, UI_OF_SLOT_MAIN_GRID_DISPIMG,  UI_MENU_ICON_DISPIMG,  "扩列图", on_dispimg);
    grid_btn(root, 235,  50, UI_OF_SLOT_MAIN_GRID_APPS,     UI_MENU_ICON_APPS,     "应用",   on_apps);
    grid_btn(root,  25, 170, UI_OF_SLOT_MAIN_GRID_FILES,    UI_MENU_ICON_FILES,    "文件",   on_files);
    grid_btn(root, 130, 170, UI_OF_SLOT_MAIN_GRID_SETTINGS, UI_MENU_ICON_SETTINGS, "设置",   on_settings);
    grid_btn(root, 235, 170, UI_OF_SLOT_MAIN_GRID_DEV,      UI_MENU_ICON_DEV,      "设备",   on_sysinfo);

    // 亮度：图标 + 滑条
    {
        lv_obj_t *ic = lv_label_create(root);
        lv_obj_set_style_text_font(ic, font_get(FONT_ICON, 28), LV_PART_MAIN | LV_STATE_DEFAULT);
        // 太阳图标跟随主强调色 (PRIMARY), 与亮度滑条一组协调
        lv_obj_set_style_text_color(ic, ui_color(UI_C_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_place(ic, 25, 290, UI_OF_SLOT_MAIN_SUN);
        lv_label_set_text(ic, UI_ICON_SUN);
    }
    self.brightness = lv_slider_create(root);
    lv_obj_set_pos(self.brightness, S(60), S(300));
    lv_obj_set_size(self.brightness, S(270), S(10));
    lv_slider_set_range(self.brightness, 1, 9);
    add_style_main_slider(self.brightness);   // 直角 + 轨道/填充/旋钮颜色 (随主题)
    ui_place(self.brightness, 60, 300, UI_OF_SLOT_MAIN_SLIDER);   // 位置/尺寸可由主题按槽位微调
    lv_obj_add_event_cb(self.brightness, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(self.brightness, on_brightness_key,
                        LV_EVENT_KEY | LV_EVENT_PREPROCESS, NULL);

    // 重启 / 关机 (各 150x50，左右 margin 25 对称，间距 10)；颜色/圆角/阴影随主题类
    {
        lv_obj_t *b = ui_text_button(root, 25, 325, 150, 50, UI_SEM_DEFAULT, "重启程序", on_restart);
        add_class(b, UI_CLS_ACTION_RESTART);
        ui_place(b, 25, 325, UI_OF_SLOT_MAIN_BTN_RESTART);
        lv_obj_t *b2 = ui_text_button(root, 185, 325, 150, 50, UI_SEM_DEFAULT, "关机", on_shutdown);
        add_class(b2, UI_CLS_ACTION_SHUTDOWN);
        ui_place(b2, 185, 325, UI_OF_SLOT_MAIN_BTN_SHUTDOWN);
    }

    // 版本号 (tick 更新，接在版权第一行后) + 版权信息
    self.version = lv_label_create(root);
    lv_obj_set_pos(self.version, S(160), S(385));
    add_style_label_small(self.version);
    lv_obj_set_style_text_color(self.version, ui_color(UI_C_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(self.version, "");
    ui_place(self.version, 160, 385, UI_OF_SLOT_MAIN_VERSION);   // 主题可微调
    {
        lv_obj_t *o = lv_label_create(root);
        lv_obj_set_pos(o, S(25), S(385));
        add_style_label_small(o);
        lv_obj_set_style_text_color(o, ui_color(UI_C_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(o,
            "电子通行证播放程序\n罗德岛工程部 白银 <inapp@iccmc.cc> Et al.2026 \n"
            "本项目是开源的自由硬件.不带任何形式的保证.\n github.com/rhodesepass");
        ui_place(o, 25, 385, UI_OF_SLOT_MAIN_COPYRIGHT);   // 主题可微调
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
