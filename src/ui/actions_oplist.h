//  干员信息 专用
#pragma once

#include "prts/prts.h"
#include "lvgl.h"

// 虚拟滚动：可见区域 + 上下缓冲
// 容器高度 280px，每项 80px，可见约 4 项，加上缓冲共 8 项
#define OPLIST_VISIBLE_SLOTS 8
#define OPLIST_ITEM_HEIGHT 80

typedef struct {
    lv_obj_t *container;  // 外层容器对象
    // ===== EEZ dirty hack 区域 - 以下四个字段顺序不可变 =====
    lv_obj_t *opbtn;      // +0: EEZ create_user_widget_operator_entry 写入
    lv_obj_t *sd_label;   // +1: EEZ create_user_widget_operator_entry 写入
    lv_obj_t *oplogo;     // +2: EEZ create_user_widget_operator_entry 写入
    lv_obj_t *opdesc;     // +3: EEZ create_user_widget_operator_entry 写入
    lv_obj_t *opname;     // +4: EEZ create_user_widget_operator_entry 写入
    // ===== EEZ dirty hack 区域结束 =====
    int operator_index;   // 该槽位当前显示的干员索引，-1 表示未使用
} ui_oplist_entry_objs_t;

typedef struct {
    prts_t* prts;
    ui_oplist_entry_objs_t slots[OPLIST_VISIBLE_SLOTS];  // 固定槽位
    int total_count;        // 干员总数
    int visible_start;      // 当前可见区域起始索引
} ui_oplist_t;


// UI层就先全局变量漫天飞吧....
extern ui_oplist_t g_ui_oplist;

// 自己添加的方法
void ui_oplist_init(prts_t* prts);
void add_oplist_btn_to_group();
void ui_oplist_focus_current_operator();
// EEZ回调不需要添加。
