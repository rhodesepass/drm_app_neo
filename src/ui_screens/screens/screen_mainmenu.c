#include "screen_mainmenu.h"

#include <string.h>

#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"
#include "ui/font_registry.h"
#include "utils/log.h"
#include "icons.h"

// 主菜单 PRTS logo (复用 EEZ 的纯图片数据资源)
LV_IMAGE_DECLARE(img_prts);

// 按钮图标实际像素 (360 基准, 随 S() 缩放, FreeType 矢量栅格化)
#define PX_BTN_ICON 40

// 本屏私有状态：只存"之后还要访问"的少数控件 + 可聚焦列表。
// 不再有全局 objects 大结构体。
static struct {
    lv_obj_t *brightness;
    lv_obj_t *version;
    lv_obj_t *focus[12];
    int       focus_cnt;
    bool      suppress_evt;  // tick 回写时抑制事件 (替代 tick_value_change_obj)
} self;

static lv_obj_t *track_focus(lv_obj_t *o)
{
    if (self.focus_cnt < (int)(sizeof(self.focus) / sizeof(self.focus[0]))) {
        self.focus[self.focus_cnt++] = o;
    }
    return o;
}

// ---- 事件回调 (即原来的 actions，现在就地、static) ----

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

// 进屏时把本屏的可聚焦控件注册到共享 group (切屏后各屏各管各的)
static void on_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    lv_group_t *g = screens_group();
    lv_group_remove_all_objs(g);
    for (int i = 0; i < self.focus_cnt; i++) {
        lv_group_add_obj(g, self.focus[i]);
    }
}

// ---- 构件 ----

static lv_obj_t *make_icon(lv_obj_t *parent, const char *glyph, int px)
{
    lv_obj_t *ic = lv_label_create(parent);
    lv_obj_set_size(ic, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(ic, font_get(FONT_ICON, px), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ic, glyph);
    return ic;
}

// 六宫格单格：图标(顶部居中, 实际大小) + 文字(底部居中)
static lv_obj_t *grid_btn(lv_obj_t *parent, int x, int y,
                          const char *icon, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(parent);
    lv_obj_set_pos(o, S(x), S(y));
    lv_obj_set_size(o, S(97), S(112));
    lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);
    add_style_main_btn(o);

    lv_obj_t *ic = make_icon(o, icon, PX_BTN_ICON);
    lv_obj_set_style_align(ic, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(ic, S(8));

    lv_obj_t *lbl = lv_label_create(o);
    lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    add_style_label_large(lbl);
    lv_obj_set_style_align(lbl, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(lbl, S(-6));
    lv_label_set_text(lbl, text);

    return track_focus(o);
}

static lv_obj_t *small_btn(lv_obj_t *parent, int x, int y, int w,
                           const char *text, lv_event_cb_t cb)
{
    lv_obj_t *o = lv_button_create(parent);
    lv_obj_set_pos(o, S(x), S(y));
    lv_obj_set_size(o, S(w), S(52));
    lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, NULL);
    add_style_main_small_btn(o);

    lv_obj_t *lbl = lv_label_create(o);
    lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    add_style_label_large(lbl);
    lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(lbl, text);

    return track_focus(o);
}

lv_obj_t *screen_mainmenu_create(void)
{
    memset(&self, 0, sizeof(self));

    lv_obj_t *root = lv_obj_create(NULL);
    lv_obj_set_size(root, S(UI_BASE_WIDTH), S(UI_BASE_HEIGHT));
    lv_obj_add_event_cb(root, on_load_start, LV_EVENT_ALL, NULL);

    // 顶部：logo + 标题
    {
        lv_obj_t *o = lv_image_create(root);
        lv_obj_set_pos(o, S(14), S(8));
        lv_image_set_src(o, &img_prts);
    }
    {
        lv_obj_t *o = lv_label_create(root);
        lv_obj_set_pos(o, S(51), S(11));
        add_style_label_large(o);
        lv_label_set_text(o, "主菜单");
    }

    // 六宫格
    grid_btn(root,  25,  48, UI_ICON_USER,          "干员",   on_oplist);
    grid_btn(root, 130,  48, UI_ICON_IMAGES,        "扩列图", on_dispimg);
    grid_btn(root, 235,  48, UI_ICON_BOX_ARCHIVE,   "应用",   on_apps);
    grid_btn(root,  25, 166, UI_ICON_FILE,          "文件",   on_files);
    grid_btn(root, 130, 166, UI_ICON_GEAR,          "设置",   on_settings);
    grid_btn(root, 235, 166, UI_ICON_MOBILE_SCREEN, "设备",   on_sysinfo);

    // 亮度：图标 + 滑条
    {
        lv_obj_t *ic = make_icon(root, UI_ICON_SUN, 28);
        lv_obj_set_pos(ic, S(25), S(285));
    }
    {
        self.brightness = lv_slider_create(root);
        lv_obj_set_pos(self.brightness, S(59), S(292));
        lv_obj_set_size(self.brightness, S(273), S(13));
        lv_slider_set_range(self.brightness, 1, 9);
        lv_obj_add_event_cb(self.brightness, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
        track_focus(self.brightness);
    }

    // 重启 / 关机
    small_btn(root,  26, 316, 148, "重启程序", on_restart);
    small_btn(root, 183, 317, 149, "关机",     on_shutdown);

    // 版本号 (tick 更新)
    {
        self.version = lv_label_create(root);
        lv_obj_set_pos(self.version, S(157), S(376));
        add_style_label_small(self.version);
        lv_label_set_text(self.version, "");
    }
    // 版权信息
    {
        lv_obj_t *o = lv_label_create(root);
        lv_obj_set_pos(o, S(23), S(376));
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
