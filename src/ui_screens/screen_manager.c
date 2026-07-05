#include "screen_manager.h"
#include "ui_plane.h"
#include "config.h"
#include "ui_metrics.h"
#include "styles.h"
#include "utils/log.h"

#include <stdint.h>

#include "screens/screen_mainmenu.h"
#include "screens/screen_oplist.h"
#include "screens/screen_sysinfo.h"
#include "screens/screen_spinner.h"
#include "screens/screen_displayimg.h"
#include "screens/screen_filemanager.h"
#include "screens/screen_settings.h"
#include "screens/screen_applist.h"
#include "screens/screen_warning.h"
#include "screens/screen_confirm.h"
#include "screens/screen_usbselect.h"

typedef lv_obj_t *(*screen_create_fn)(void);
typedef void (*screen_tick_fn)(void);

typedef struct {
    screen_create_fn create;
    screen_tick_fn   tick;
    lv_obj_t        *obj;   // 懒创建后缓存
    int              panel_y; // 该屏 UI 平面在面板上的停靠 Y (DEBE 图层坐标)
} screen_entry_t;

static screen_entry_t s_screens[SCREEN_COUNT];
static screen_id_t    s_current = SCREEN_SPINNER;
static lv_group_t    *s_group;

// ---- 平台服务钩子：弱符号默认空实现，设备侧覆盖 ----
__attribute__((weak)) void ui_hook_shutdown_request(void)
{
    log_debug("[nav] shutdown request (no platform handler)");
}
__attribute__((weak)) void ui_hook_displayimg_key(uint32_t key)
{
    (void)key;
}
__attribute__((weak)) void ui_hook_restart(void)
{
    log_debug("[nav] restart request (no platform handler)");
}
__attribute__((weak)) void ui_hook_format_sd(void)
{
    log_debug("[nav] format SD request (no platform handler)");
}
__attribute__((weak)) void ui_hook_srgn_config(void)
{
    log_debug("[nav] srgn config request (no platform handler)");
}
__attribute__((weak)) void ui_hook_filemanager_mount(lv_obj_t *container)
{
    (void)container;
}
__attribute__((weak)) void ui_hook_filemanager_enter(lv_group_t *group)
{
    (void)group;
}

lv_group_t *screens_group(void) { return s_group; }
screen_id_t screens_current(void) { return s_current; }

static void register_screens(void)
{
    // 停靠 Y：mainmenu/oplist 是"滑到下方、上方露出立绘"的卡片；
    // 其余屏停靠 0 (满屏)；spinner 停在 SCREEN_HEIGHT (藏到屏幕下方)。
    s_screens[SCREEN_MAINMENU]   = (screen_entry_t){ screen_mainmenu_create,   screen_mainmenu_tick,   NULL, UI_MAINMENU_Y };
    s_screens[SCREEN_OPLIST]     = (screen_entry_t){ screen_oplist_create,     NULL,                   NULL, UI_OPLIST_Y };
    s_screens[SCREEN_SYSINFO]    = (screen_entry_t){ screen_sysinfo_create,    screen_sysinfo_tick,    NULL, 0 };
    s_screens[SCREEN_SPINNER]    = (screen_entry_t){ screen_spinner_create,    NULL,                   NULL, SCREEN_HEIGHT };
    s_screens[SCREEN_DISPLAYIMG] = (screen_entry_t){ screen_displayimg_create, screen_displayimg_tick, NULL, 0 };
    s_screens[SCREEN_FILEMANAGER]= (screen_entry_t){ screen_filemanager_create,NULL,                   NULL, 0 };
    s_screens[SCREEN_SETTINGS]   = (screen_entry_t){ screen_settings_create,   screen_settings_tick,   NULL, 0 };
    s_screens[SCREEN_APPLIST]    = (screen_entry_t){ screen_applist_create,    screen_applist_tick,    NULL, 0 };
    s_screens[SCREEN_WARNING]    = (screen_entry_t){ screen_warning_create,    NULL,                   NULL, UI_WARNING_Y };
    s_screens[SCREEN_CONFIRM]    = (screen_entry_t){ screen_confirm_create,    NULL,                   NULL, UI_CONFIRM_Y };
    s_screens[SCREEN_USBSELECT]  = (screen_entry_t){ screen_usbselect_create,  NULL,                   NULL, UI_USBSELECT_Y };
}

// 瞬切内容，不做 LVGL 软件过渡。过渡观感交给硬件图层 Y 滑动(ui_plane_move)：
// UI 是单 buffer 直绘，LVGL 的 fade/move 动画会连续多帧全屏重绘，撞上 DEBE 扫描
// 会持续撕裂；DEBE 图层滑动只挪坐标不重绘 FB，才是无撕裂的过渡。
static void load_now(screen_id_t id)
{
    if (s_screens[id].obj) {
        lv_screen_load(s_screens[id].obj);
    }
}

// ---- 过渡幕帘 ----
// partial 单 buffer 下全屏重绘有肉眼可见的自上而下刷新波前。遮盖办法：所有切屏
// 统一走"下潜到只露幕帘条 → 藏着 lv_refr_now 同步画完 → 回升"(原 spinner intro
// 的推广)。幕帘挂在 lv_layer_top()，lv_screen_load 换屏不销毁；停靠 UI_SPINNER_INTRO_Y
// 时屏上只剩这条(像素不变)，底下换什么都看不见。
static lv_obj_t  *s_curtain;
static lv_timer_t *s_swap_timer;   // 在途的换内容定时器 (dip 到位后触发)

static void curtain_build(void)
{
    s_curtain = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(s_curtain, 0, 0);
    lv_obj_set_size(s_curtain, S(UI_BASE_WIDTH), SCREEN_HEIGHT - UI_SPINNER_INTRO_Y);
    lv_obj_set_style_pad_all(s_curtain, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_curtain, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_curtain, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    add_style_screen_bg(s_curtain);   // 不透明底，盖住底下的换屏重绘

    // 内容与 screen_spinner 顶条一致："正在提交反馈至神经" 过场观感
    lv_obj_t *sp = lv_spinner_create(s_curtain);
    lv_obj_set_pos(sp, S(20), S(5));
    lv_obj_set_size(sp, S(44), S(44));
    lv_obj_set_style_arc_width(sp, S(6), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(sp, S(6), LV_PART_MAIN | LV_STATE_DEFAULT);
    add_style_spinner_arc(sp);

    lv_obj_t *status = lv_label_create(s_curtain);
    lv_obj_set_pos(status, S(80), S(24));
    add_style_label_small(status);
    lv_label_set_text(status, "正在提交反馈至神经...");

    lv_obj_t *log = lv_label_create(s_curtain);
    lv_obj_set_pos(log, S(230), S(0));
    add_style_label_small(log);
    add_style_log_text(log);
    lv_label_set_text(log,
        "RDEP Connection\n==> Rhodes Island\n[OK] TLS Handshake\n[OK] ::43232->::22");

    lv_obj_add_flag(s_curtain, LV_OBJ_FLAG_HIDDEN);
}

static void curtain_show(void)
{
    if (!s_curtain) curtain_build();
    lv_anim_delete(s_curtain, NULL);   // 收帘动画在途时被再次触发：先复位
    lv_obj_set_y(s_curtain, 0);
    lv_obj_remove_flag(s_curtain, LV_OBJ_FLAG_HIDDEN);
}

static void curtain_retract_done_cb(lv_anim_t *a)
{
    (void)a;
    lv_obj_add_flag(s_curtain, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(s_curtain, 0);
}

// 回升到位后把幕帘往上滑出画面再隐藏，避免顶条内容瞬间跳变。
// 收帘是小面积软件动画(仅幕帘条高)，每帧一个 partial 块，撕裂可忽略。
static void curtain_retract(uint32_t delay_ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_curtain);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, 0, -(SCREEN_HEIGHT - UI_SPINNER_INTRO_Y));
    lv_anim_set_duration(&a, UI_TRANSITION_CURTAIN_RETRACT_MS);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, curtain_retract_done_cb);
    lv_anim_start(&a);
}

// dip 到位：幕帘遮着换内容并同步画完(重绘波前发生在此刻，屏上只剩幕帘)，再回升。
static void swap_cb(lv_timer_t *t)
{
    screen_id_t id = (screen_id_t)(intptr_t)lv_timer_get_user_data(t);
    s_swap_timer = NULL;
    load_now(id);
    lv_refr_now(NULL);
    ui_plane_move(UI_SPINNER_INTRO_Y, s_screens[id].panel_y,
                  UI_TRANSITION_RISE_DURATION, 0);
    curtain_retract(UI_TRANSITION_RISE_DURATION / 1000);
}

void screens_init(void)
{
    s_group = lv_group_create();
    register_screens();

    // 首屏 spinner：UI 平面停在 SCREEN_HEIGHT (隐藏)，与 drm_warpper mount 一致。
    s_current = SCREEN_SPINNER;
    if (!s_screens[SCREEN_SPINNER].obj && s_screens[SCREEN_SPINNER].create) {
        s_screens[SCREEN_SPINNER].obj = s_screens[SCREEN_SPINNER].create();
    }
    load_now(SCREEN_SPINNER);
}

void screen_show(screen_id_t id)
{
    if (id < 0 || id >= SCREEN_COUNT || !s_screens[id].create) {
        log_warn("screen_show: screen %d not registered (skeleton)", (int)id);
        return;
    }
    if (id == s_current) {
        return;
    }

    // 导航按钮用 LV_EVENT_PRESSED 触发切屏，切走后该按钮收不到 RELEASED，
    // pressed(黑)态会残留到这屏下次显示(屏对象缓存复用不销毁)。切屏前手动把
    // 当前按下项(encoder 下即 group focused)的 pressed 清掉，并复位 indev，
    // 免得这次按压的 release 落到新屏上误触发。
    lv_obj_t *pressed = lv_group_get_focused(s_group);
    if (pressed) {
        lv_obj_remove_state(pressed, LV_STATE_PRESSED);
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev) {
        lv_indev_reset(indev, NULL);
    }

    if (!s_screens[id].obj) {
        s_screens[id].obj = s_screens[id].create();
    }

    // 统一两段式：下潜到只露幕帘 → swap_cb 藏着换内容 → 回升到目标停靠 Y。
    // from==spinner 时 from_y=SCREEN_HEIGHT，第一段就是原 intro 的上浮，同一段代码。
    curtain_show();
    if (!s_swap_timer) {
        ui_plane_move(s_screens[s_current].panel_y, UI_SPINNER_INTRO_Y,
                      UI_TRANSITION_DIP_DURATION, 0);
        s_swap_timer = lv_timer_create(swap_cb, UI_TRANSITION_DIP_DURATION / 1000,
                                       (void *)(intptr_t)id);
        lv_timer_set_repeat_count(s_swap_timer, 1); // 一次性，触发后自动删除
    } else {
        // 上一次过渡还在下潜途中：只改目的地，不再叠一段动画
        lv_timer_set_user_data(s_swap_timer, (void *)(intptr_t)id);
    }

    s_current = id;
}

void screens_tick(void)
{
    if (s_screens[s_current].tick) {
        s_screens[s_current].tick();
    }
}

void screens_rebuild(screen_id_t id)
{
    if (id < 0 || id >= SCREEN_COUNT || !s_screens[id].obj) {
        return;
    }
    bool was_current = (s_current == id);
    lv_obj_delete(s_screens[id].obj);
    s_screens[id].obj = s_screens[id].create();
    if (was_current) {
        load_now(id);
    }
}

// ============ 按键导航状态机 (原 scr_transition.c::screen_key_event_cb) ============
//
// 平台无关的导航策略；屏切换统一走 screen_show，平台服务走 ui_hook_*。
// 注: WARNING / CONFIRM 在新架构里按"模态服务"处理，铺到那步再补对应分支。

void screens_handle_key(uint32_t key)
{
    if (key == LV_KEY_END) {
        ui_hook_shutdown_request();
    }

    // 扩列图：3/4 键(ESC/ENTER)关闭回 spinner，其余键交给扩列图自身
    if (s_current == SCREEN_DISPLAYIMG) {
        if (key == LV_KEY_ESC || key == LV_KEY_ENTER) {
            screen_show(SCREEN_SPINNER);
        } else {
            ui_hook_displayimg_key(key);
        }
        return;
    }

    // 确认框：ESC 等同取消，回 spinner
    if (s_current == SCREEN_CONFIRM) {
        if (key == LV_KEY_ESC) {
            screen_confirm_escape();
        }
        return;
    }

    // 主菜单 / 干员列表：ESC 回 spinner
    if (s_current == SCREEN_MAINMENU || s_current == SCREEN_OPLIST) {
        if (key == LV_KEY_ESC) {
            screen_show(SCREEN_SPINNER);
        }
        return;
    }

    // 其他屏(非 spinner)：非编辑态下 ESC 回主菜单，否则退出编辑态
    if (s_current != SCREEN_SPINNER) {
        bool is_editing = lv_group_get_editing(s_group);
        if (key == LV_KEY_ESC) {
            if (!is_editing) {
                screen_show(SCREEN_MAINMENU);
            } else {
                lv_group_set_editing(s_group, false);
            }
        }
        return;
    }

    // spinner(空界面)：方向键出干员列表，回车出扩列图，ESC 出主菜单
    switch (key) {
        case LV_KEY_LEFT:
        case LV_KEY_RIGHT:
            screen_show(SCREEN_OPLIST);
            break;
        case LV_KEY_ENTER:
            screen_show(SCREEN_DISPLAYIMG);
            break;
        case LV_KEY_ESC:
            screen_show(SCREEN_MAINMENU);
            break;
        default:
            break;
    }
}
