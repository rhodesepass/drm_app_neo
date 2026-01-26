// 应用管理 专用

#include <apps/apps.h>
#include <stdbool.h>
#include <stdint.h>
#include <ui/actions_warning.h>

#include "styles.h"
#include "ui.h"
#include "ui/actions_apps.h"
#include "utils/log.h"
#include "ui/scr_transition.h"
#include "config.h"
#include "utils/spsc_queue.h"
#include "lvgl.h"
#include "apps/apps_types.h"

ui_apps_t g_ui_apps;
static lv_timer_t *g_ui_apps_timer = NULL;
// =========================================
// 自己添加的方法 START
// =========================================

// 防递归标志 - 防止虚拟滚动时焦点事件递归触发
static bool g_scroll_in_progress = false;

// 前向声明
static void update_slot_content(int slot_idx, int app_idx);
static void update_visible_range(int new_start);
static void apps_focus_cb(lv_event_t *e);
static void refocus_to_app(int app_idx);

static void app_btn_click_cb(lv_event_t *e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);

    // 从 user_data 获取APP
    int app_idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_entry_t *app = &g_ui_apps.apps->apps[app_idx];

    if(app->type == APP_TYPE_FOREGROUND_EXTENSION_ONLY){
        // 前台应用，但只能通过扩展名启动。所以直接告警
        ui_warning(UI_WARNING_APP_NO_DIRECT_START);
        ui_schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
        return;
    }
    else if (app->type == APP_TYPE_FOREGROUND){
        // 前台应用，直接启动
        apps_try_launch_by_index(g_ui_apps.apps, app_idx);
        ui_schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
        return;
    }
    else if (app->type == APP_TYPE_BACKGROUND){
        // 后台应用，切换状态
        apps_toggle_bg_app_by_index(g_ui_apps.apps, app_idx);
        ui_schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
        return;
    }
    else{
        log_error("unknown app type: %d", app->type);
    }
}

// 根据应用索引重新设置焦点（内部使用，用于虚拟滚动后保持焦点）
static void refocus_to_app(int app_idx) {
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        if (g_ui_apps.slots[i].app_index == app_idx) {
            lv_group_focus_obj(g_ui_apps.slots[i].appbtn);
            return;
        }
    }
}


// 创建单个槽位的 UI 对象
static void create_slot_ui(int slot_idx) {
    ui_apps_entry_objs_t *slot = &g_ui_apps.slots[slot_idx];

    // 创建外层容器 stolen from eez generated code.
    lv_obj_t *obj = lv_obj_create(objects.app_container);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, LV_PCT(97), UI_APP_ITEM_HEIGHT);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    slot->container = obj;

    // 我们这里要用EEZ 生成的
    // void create_user_widget_app_entry(lv_obj_t *parent_obj, int startWidgetIndex);
    // 来创建干员列表项。但是它startWidgetIndex是相对于eez的objects的。
    // 我们希望添加到自己的信息表里。因此，这里（又！）有一个非常，非常，非常，非常Dirty的hacks

    // create_user_widget_app_entry的写入方法是：
    // ((lv_obj_t **)&objects)[startWidgetIndex + 0] = obj;
    // 也就是*((lv_obj_t **)&objects) + startWidgetIndex) = obj;
    // 令 startWidgetIndex = (lv_obj_t**)&slot->appbtn - (lv_obj_t **)&objects;
    // 这样实际的操作就是  *((lv_obj_t**)&slot->appbtn) = obj，即slot->appbtn = obj
    #warning "Dirty hacks happened here(AGAIN!). If application crash during app->ui_apps sync, Please check alignness and such."
    int startWidgetIndex = (lv_obj_t**)&slot->appbtn - (lv_obj_t **)&objects;
    create_user_widget_app_entry(obj, startWidgetIndex);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    add_style_op_entry(obj);

    slot->app_index = -1;  // 初始未绑定应用
}

// 更新槽位内容为指定应用
static void update_slot_content(int slot_idx, int app_idx) {
    ui_apps_entry_objs_t *slot = &g_ui_apps.slots[slot_idx];
    app_entry_t *app = &g_ui_apps.apps->apps[app_idx];

    lv_label_set_text(slot->appname, app->app_name);
    lv_label_set_text(slot->appdesc, app->description);
    lv_image_set_src(slot->applogo, app->icon_path);

    // SD标记可见性
    if (app->source == APP_SOURCE_NAND) {
        lv_obj_add_flag(slot->sd_flag, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(slot->sd_flag, LV_OBJ_FLAG_HIDDEN);
    }

    // 前/后台标记：根据app类型设置文本和style
    if (app->type == APP_TYPE_BACKGROUND) {
        lv_label_set_text(slot->bgfg_flag, "后台");
        if(app->pid != -1){
            add_style_app_bg_running(slot->bgfg_flag);
        }
        else{
            add_style_app_bg_notrunning(slot->bgfg_flag);
        }
    } else {
        lv_label_set_text(slot->bgfg_flag, "前台");
        add_style_app_fg(slot->bgfg_flag);
    }

    lv_obj_remove_event_cb(slot->appbtn, app_btn_click_cb);
    lv_obj_add_event_cb(slot->appbtn, app_btn_click_cb, LV_EVENT_PRESSED, (void *)(intptr_t)app_idx);

    slot->app_index = app_idx;
}

// 更新可见区域
static void update_visible_range(int new_start) {
    if (new_start < 0) new_start = 0;
    int max_start = g_ui_apps.total_count - UI_APP_VISIBLE_SLOTS;
    if (max_start < 0) max_start = 0;
    if (new_start > max_start) new_start = max_start;

    int old_start = g_ui_apps.visible_start;
    if (new_start == old_start) return;

    g_ui_apps.visible_start = new_start;

    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        int idx = new_start + i;
        if (idx < g_ui_apps.total_count) {
            update_slot_content(i, idx);
            lv_obj_remove_flag(g_ui_apps.slots[i].container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_ui_apps.slots[i].container, LV_OBJ_FLAG_HIDDEN);
            g_ui_apps.slots[i].app_index = -1;
        }
    }
}

// 焦点变化回调 - encoder导航驱动的虚拟滚动
static void apps_focus_cb(lv_event_t *e) {
    if (g_scroll_in_progress) return;

    lv_obj_t *focused = lv_event_get_target(e);

    int slot_idx = -1;
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        if (g_ui_apps.slots[i].appbtn == focused) {
            slot_idx = i;
            break;
        }
    }
    if (slot_idx < 0) return;

    int app_idx = g_ui_apps.slots[slot_idx].app_index;
    if (app_idx < 0) return;

    if (slot_idx <= 1 && g_ui_apps.visible_start > 0) {
        g_scroll_in_progress = true;
        update_visible_range(g_ui_apps.visible_start - 1);
        refocus_to_app(app_idx);
        g_scroll_in_progress = false;
    } else if (slot_idx >= UI_APP_VISIBLE_SLOTS - 2 &&
               g_ui_apps.visible_start + UI_APP_VISIBLE_SLOTS < g_ui_apps.total_count) {
        g_scroll_in_progress = true;
        update_visible_range(g_ui_apps.visible_start + 1);
        refocus_to_app(app_idx);
        g_scroll_in_progress = false;
    }
}

// lvgl 定时器回调，更新当前slot的 后台程序 是否在运行
static void ui_apps_timer_cb(lv_timer_t * timer){
    // 防御性检查：如果 apps 已经被销毁，直接返回
    if (g_ui_apps.apps == NULL) {
        return;
    }
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        int app_idx = g_ui_apps.slots[i].app_index;
        if (app_idx >= 0) {
            app_entry_t *app = &g_ui_apps.apps->apps[app_idx];
            if (app->type == APP_TYPE_BACKGROUND) {
                if (app->pid != -1) {
                    add_style_app_bg_running(g_ui_apps.slots[i].bgfg_flag);
                }
                else{
                    add_style_app_bg_notrunning(g_ui_apps.slots[i].bgfg_flag);
                }
            }
        }
    }
}

void ui_apps_init(apps_t *apps){
    g_ui_apps.apps = apps;
    g_ui_apps.total_count = apps->app_count;
    g_ui_apps.visible_start = 0;

    log_info("START apps->ui sync (virtual scroll mode)!! Total apps: %d", apps->app_count);
    // 清空应用列表容器
    lv_obj_clean(objects.app_container);

    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        create_slot_ui(i);
        if (i < apps->app_count) {
            update_slot_content(i, i);
            lv_obj_remove_flag(g_ui_apps.slots[i].container, LV_OBJ_FLAG_HIDDEN);

            // 只为有效的槽位注册焦点回调
            lv_obj_add_event_cb(g_ui_apps.slots[i].appbtn,
                                apps_focus_cb, LV_EVENT_FOCUSED, NULL);
        } else {
            lv_obj_add_flag(g_ui_apps.slots[i].container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    g_ui_apps_timer = lv_timer_create(ui_apps_timer_cb, APPS_BG_APP_CHECK_PERIOD / 1000, NULL);

    log_info("apps->ui sync complete! Created %d slots for %d apps",
             UI_APP_VISIBLE_SLOTS, apps->app_count);
}

void add_applist_btn_to_group() {
    lv_group_remove_all_objs(groups.op);

    // 禁用 wrap 循环，避免边界槽位间反复跳动
    lv_group_set_wrap(groups.op, false);

    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        if (g_ui_apps.slots[i].app_index >= 0) {
            lv_group_add_obj(groups.op, g_ui_apps.slots[i].appbtn);
        }
    }
    lv_group_add_obj(groups.op, objects.applist_back_btn);
}

void ui_apps_destroy(){
    if (g_ui_apps_timer != NULL) {
        lv_timer_delete(g_ui_apps_timer);
        g_ui_apps_timer = NULL;
    }
    // 将 apps 指针设置为 NULL，防止定时器回调访问已销毁的数据
    g_ui_apps.apps = NULL;
}

// =========================================
// EEZ 回调 START
// =========================================

bool get_var_applist_show_warning(){
    return g_ui_apps.total_count != 0;
}
void set_var_applist_show_warning(bool value){
    return;
}