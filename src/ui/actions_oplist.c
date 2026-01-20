// 干员信息 专用 - 虚拟滚动实现（

#include <src/core/lv_obj_event.h>
#include <src/core/lv_obj_private.h>
#include <src/misc/lv_event.h>
#include <stdint.h>
#include <stdlib.h>

#include "ui.h"
#include "utils/log.h"
#include "prts/prts.h"
#include "ui/actions_oplist.h"
#include "styles.h"
#include "ui/scr_transition.h"


ui_oplist_t g_ui_oplist;
extern objects_t objects;

// 前向声明
static void update_slot_content(int slot_idx, int operator_idx);
static void oplist_focus_cb(lv_event_t *e);
static void refocus_to_operator(int op_idx);

// 根据干员索引重新设置焦点
static void refocus_to_operator(int op_idx) {
    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        if (g_ui_oplist.slots[i].operator_index == op_idx) {
            lv_group_focus_obj(g_ui_oplist.slots[i].opbtn);
            return;
        }
    }
}

static void op_btn_click_cb(lv_event_t *e){
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);

    // 从 user_data 获取干员索引
    int op_idx = (int)(intptr_t)lv_event_get_user_data(e);
    prts_t *prts = g_ui_oplist.prts;

    prts_request_set_operator(prts, op_idx);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
}

// 创建单个槽位的 UI 对象
static void create_slot_ui(int slot_idx) {
    ui_oplist_entry_objs_t *slot = &g_ui_oplist.slots[slot_idx];

    // 创建外层容器
    lv_obj_t *obj = lv_obj_create(objects.oplst_container);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, LV_PCT(97), OPLIST_ITEM_HEIGHT);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    slot->container = obj;

    // 使用 EEZ 的 dirty hack 来创建子对象
    #warning "Dirty hacks happened here. If application crash during prts->ui sync, Please check alignness and such."
    int startWidgetIndex = (lv_obj_t**)&slot->opbtn - (lv_obj_t **)&objects;
    create_user_widget_operator_entry(obj, startWidgetIndex);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    add_style_op_entry(obj);

    slot->operator_index = -1;  // 初始未绑定干员
}

// 更新槽位内容为指定干员
static void update_slot_content(int slot_idx, int operator_idx) {
    ui_oplist_entry_objs_t *slot = &g_ui_oplist.slots[slot_idx];
    prts_operator_entry_t *op = &g_ui_oplist.prts->operators[operator_idx];

    // 更新内容
    lv_label_set_text(slot->opname, op->operator_name);
    lv_label_set_text(slot->opdesc, op->description);
    lv_image_set_src(slot->oplogo, op->icon_path);

    // 移除旧的事件回调，添加新的
    lv_obj_remove_event_cb(slot->opbtn, op_btn_click_cb);
    lv_obj_add_event_cb(slot->opbtn, op_btn_click_cb, LV_EVENT_PRESSED, (void*)(intptr_t)operator_idx);

    slot->operator_index = operator_idx;
}

// 更新可见区域
static void update_visible_range(int new_start) {
    // 边界检查
    if (new_start < 0) new_start = 0;
    int max_start = g_ui_oplist.total_count - OPLIST_VISIBLE_SLOTS;
    if (max_start < 0) max_start = 0;
    if (new_start > max_start) new_start = max_start;

    int old_start = g_ui_oplist.visible_start;
    if (new_start == old_start) return;

    g_ui_oplist.visible_start = new_start;

    // 更新所有槽位
    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        int op_idx = new_start + i;
        if (op_idx < g_ui_oplist.total_count) {
            update_slot_content(i, op_idx);
            lv_obj_remove_flag(g_ui_oplist.slots[i].container, LV_OBJ_FLAG_HIDDEN);
        } else {
            // 隐藏多余的槽位
            lv_obj_add_flag(g_ui_oplist.slots[i].container, LV_OBJ_FLAG_HIDDEN);
            g_ui_oplist.slots[i].operator_index = -1;
        }
    }
}

// 焦点变化回调 - encoder导航驱动的虚拟滚动
static void oplist_focus_cb(lv_event_t *e) {
    lv_obj_t *focused = lv_event_get_target(e);

    // 找到当前焦点的slot索引
    int slot_idx = -1;
    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        if (g_ui_oplist.slots[i].opbtn == focused) {
            slot_idx = i;
            break;
        }
    }
    if (slot_idx < 0) return;

    int op_idx = g_ui_oplist.slots[slot_idx].operator_index;
    if (op_idx < 0) return;

    // 边界检测：焦点移到顶部附近，向上滚动
    if (slot_idx <= 1 && g_ui_oplist.visible_start > 0) {
        int new_start = g_ui_oplist.visible_start - 1;
        update_visible_range(new_start);
        refocus_to_operator(op_idx);
    }
    // 边界检测：焦点移到底部附近，向下滚动
    else if (slot_idx >= OPLIST_VISIBLE_SLOTS - 2 &&
             g_ui_oplist.visible_start + OPLIST_VISIBLE_SLOTS < g_ui_oplist.total_count) {
        int new_start = g_ui_oplist.visible_start + 1;
        update_visible_range(new_start);
        refocus_to_operator(op_idx);
    }
}

//自己添加的方法
void ui_oplist_init(prts_t* prts){
    g_ui_oplist.prts = prts;
    g_ui_oplist.total_count = prts->operator_count;
    g_ui_oplist.visible_start = 0;

    log_info("START prts->ui sync (virtual scroll mode)!! Total operators: %d", prts->operator_count);

    // 清空干员列表容器
    lv_obj_clean(objects.oplst_container);

    // 创建固定数量的槽位
    int slots_to_create = OPLIST_VISIBLE_SLOTS;
    if (slots_to_create > prts->operator_count) {
        slots_to_create = prts->operator_count;
    }

    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        create_slot_ui(i);
        if (i < prts->operator_count) {
            update_slot_content(i, i);
            lv_obj_remove_flag(g_ui_oplist.slots[i].container, LV_OBJ_FLAG_HIDDEN);

            // 只为有效的槽位注册焦点回调
            lv_obj_add_event_cb(g_ui_oplist.slots[i].opbtn,
                                oplist_focus_cb, LV_EVENT_FOCUSED, NULL);
        } else {
            lv_obj_add_flag(g_ui_oplist.slots[i].container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    log_info("prts->ui sync complete! Created %d slots for %d operators",
             slots_to_create, prts->operator_count);
}

void add_oplist_btn_to_group(){
    lv_group_remove_all_objs(groups.op);

    // 只添加当前可见的按钮到组
    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        if (g_ui_oplist.slots[i].operator_index >= 0) {
            lv_group_add_obj(groups.op, g_ui_oplist.slots[i].opbtn);
        }
    }
    lv_group_add_obj(groups.op, objects.mainmenu_btn);
}

void ui_oplist_focus_current_operator(){
    int current_op = g_ui_oplist.prts->operator_index;

    // 确保当前干员在可见范围内
    if (current_op < g_ui_oplist.visible_start ||
        current_op >= g_ui_oplist.visible_start + OPLIST_VISIBLE_SLOTS) {
        // 滚动到当前干员
        update_visible_range(current_op);
    }

    // 找到对应的槽位并聚焦
    for (int i = 0; i < OPLIST_VISIBLE_SLOTS; i++) {
        if (g_ui_oplist.slots[i].operator_index == current_op) {
            lv_group_focus_obj(g_ui_oplist.slots[i].opbtn);
            return;
        }
    }
}
