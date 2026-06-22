#include "screen_manager.h"
#include "ui_plane.h"
#include "config.h"
#include "utils/log.h"

#include <stdint.h>

#include "screens/screen_mainmenu.h"
// 铺其余屏时在此 include 各 screen_xxx.h

typedef lv_obj_t *(*screen_create_fn)(void);
typedef void (*screen_tick_fn)(void);

typedef struct {
    screen_create_fn create;
    screen_tick_fn   tick;
    lv_obj_t        *obj;   // 懒创建后缓存
    int              panel_y; // 该屏 UI 平面在面板上的停靠 Y (DEBE 图层坐标)
} screen_entry_t;

static screen_entry_t s_screens[SCREEN_COUNT];
static screen_id_t    s_current = SCREEN_MAINMENU;
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

lv_group_t *screens_group(void) { return s_group; }
screen_id_t screens_current(void) { return s_current; }

static void register_screens(void)
{
    // 停靠 Y：mainmenu/oplist 是"滑到下方、上方露出立绘"的卡片；
    // 其余屏停靠 0 (满屏)；spinner 停在 SCREEN_HEIGHT (藏到屏幕下方)。
    s_screens[SCREEN_MAINMENU]   = (screen_entry_t){ screen_mainmenu_create, screen_mainmenu_tick, NULL, UI_MAINMENU_Y };
    s_screens[SCREEN_OPLIST]     = (screen_entry_t){ NULL, NULL, NULL, UI_OPLIST_Y };
    s_screens[SCREEN_SYSINFO]    = (screen_entry_t){ NULL, NULL, NULL, 0 };
    s_screens[SCREEN_SPINNER]    = (screen_entry_t){ NULL, NULL, NULL, SCREEN_HEIGHT };
    s_screens[SCREEN_DISPLAYIMG] = (screen_entry_t){ NULL, NULL, NULL, 0 };
    s_screens[SCREEN_FILEMANAGER]= (screen_entry_t){ NULL, NULL, NULL, 0 };
    s_screens[SCREEN_SETTINGS]   = (screen_entry_t){ NULL, NULL, NULL, 0 };
    s_screens[SCREEN_APPLIST]    = (screen_entry_t){ NULL, NULL, NULL, 0 };
}

static void load_now(screen_id_t id)
{
    if (s_screens[id].obj) {
        lv_screen_load_anim(s_screens[id].obj, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

static void delayed_load_cb(lv_timer_t *t)
{
    screen_id_t id = (screen_id_t)(intptr_t)lv_timer_get_user_data(t);
    load_now(id);
}

static void schedule_load(screen_id_t id, uint32_t delay_ms)
{
    lv_timer_t *t = lv_timer_create(delayed_load_cb, delay_ms, (void *)(intptr_t)id);
    lv_timer_set_repeat_count(t, 1); // 一次性，触发后自动删除
}

void screens_init(void)
{
    s_group = lv_group_create();
    register_screens();

    // 首屏直接显示，不放过渡 (设备侧若从 spinner 起步可改为 screen_show)
    s_current = SCREEN_MAINMENU;
    if (!s_screens[SCREEN_MAINMENU].obj && s_screens[SCREEN_MAINMENU].create) {
        s_screens[SCREEN_MAINMENU].obj = s_screens[SCREEN_MAINMENU].create();
    }
    load_now(SCREEN_MAINMENU);
}

void screen_show(screen_id_t id)
{
    if (id < 0 || id >= SCREEN_COUNT || !s_screens[id].create) {
        log_warn("screen_show: screen %d not registered (skeleton)", (int)id);
        return;
    }
    if (!s_screens[id].obj) {
        s_screens[id].obj = s_screens[id].create();
    }

    int from_y = s_screens[s_current].panel_y;
    int to_y   = s_screens[id].panel_y;

    // 从 spinner(隐藏) 进任意屏：两段式 —— 先快速滑到 INTRO_Y，再滑到目标 Y(带延时)，
    // 内容在延时后切换，与原 scr_transition 一致。
    if (s_current == SCREEN_SPINNER) {
        ui_plane_move(SCREEN_HEIGHT, UI_SPINNER_INTRO_Y,
                      UI_LAYER_ANIMATION_INTRO_SPINNER_TRANSITION_DURATION, 0);
        ui_plane_move(UI_SPINNER_INTRO_Y, to_y,
                      UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DURATION,
                      UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DELAY);
        schedule_load(id, UI_LAYER_ANIMATION_INTRO_LOADSCREEN_DELAY / 1000);
    } else {
        if (from_y != to_y) {
            ui_plane_move(from_y, to_y, UI_LAYER_ANIMATION_DURATION, 0);
        }
        load_now(id);
    }

    s_current = id;
}

void screens_tick(void)
{
    if (s_screens[s_current].tick) {
        s_screens[s_current].tick();
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
