#include "screen_oplist.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"

// 干员列表：焦点驱动的虚拟滚动 —— 只建 UI_OPLIST_VISIBLE_SLOTS 个槽位循环复用，
// 不随干员数线性增长 (弱端省 RAM)。移植自原 actions_oplist.c。
typedef struct {
    lv_obj_t *cont;     // 槽位外层(决定行高/间距)
    lv_obj_t *btn;      // 可聚焦按钮
    lv_obj_t *logo;
    lv_obj_t *name;
    lv_obj_t *desc;
    lv_obj_t *sd;
    lv_obj_t *res; // 分辨率角标 (SD 上方)
    int       op_index; // 当前绑定干员，-1=空
} oplist_slot_t;

static struct {
    lv_obj_t     *scroll;   // 滚动容器(承载所有槽位)
    lv_obj_t     *menu_btn;
    lv_obj_t     *refresh_btn;
    oplist_slot_t slots[UI_OPLIST_VISIBLE_SLOTS];
    int           total;
    int           visible_start;
    bool          scroll_guard; // 防焦点->滚动->焦点 递归
} self;

// 排序模式：长按确定"拎起"当前干员，左右键在列表里上下移动，短按确定/ESC 落位。
// 被拎起的项靠 group editing 态下聚焦项自动进入的 LV_STATE_EDITED 高亮，随移动自动跟随。
static bool s_reorder_active = false;
static int  s_reorder_op     = -1;

static void update_visible_range(int new_start);
static void refocus_op(int op_index);

static void on_menu(lv_event_t *e)    { (void)e; screen_show(SCREEN_MAINMENU); }
static void on_refresh(lv_event_t *e)
{
    (void)e;
    ui_backend_oplist_refresh();
    screen_show(SCREEN_SPINNER);
}

static void refocus_op(int op_index)
{
    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++)
        if (self.slots[i].op_index == op_index) { lv_group_focus_obj(self.slots[i].btn); return; }
}

static void build_group(int focus_op);

// 排序模式下：让导航 group 只留"当前显示 s_reorder_op 的槽"，LVGL 焦点无处可移
// (方向键交给 screens_handle_key 做移动)；被拎项打 USER_1 高亮。
static void reorder_focus_picked(void)
{
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++)
        lv_obj_remove_state(self.slots[i].btn, LV_STATE_USER_1);
    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++) {
        if (self.slots[i].op_index == s_reorder_op) {
            lv_group_add_obj(g, self.slots[i].btn);
            lv_group_focus_obj(self.slots[i].btn);
            lv_obj_add_state(self.slots[i].btn, LV_STATE_USER_1);
            lv_obj_scroll_to_view_recursive(self.slots[i].btn, LV_ANIM_OFF);
            break;
        }
    }
}

static void reorder_enter(int op_index)
{
    s_reorder_active = true;
    s_reorder_op = op_index;
    reorder_focus_picked();
}

static void reorder_exit(bool save)
{
    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++)
        lv_obj_remove_state(self.slots[i].btn, LV_STATE_USER_1);
    int op = s_reorder_op;
    s_reorder_active = false;
    s_reorder_op = -1;
    build_group(op);              // 恢复完整导航 group，焦点停在落位的干员
    if (save) ui_backend_oplist_save_order();
}

bool screen_oplist_is_reordering(void) { return s_reorder_active; }
void screen_oplist_exit_reorder(bool save) { if (s_reorder_active) reorder_exit(save); }

// 排序模式下方向键：把被拎干员上移(LEFT)/下移(RIGHT)一格。由 screen_manager 调用。
void screen_oplist_reorder_move(uint32_t key)
{
    if (!s_reorder_active) return;
    int from = s_reorder_op;
    int to;
    if (key == LV_KEY_LEFT)       to = from - 1;
    else if (key == LV_KEY_RIGHT) to = from + 1;
    else return;
    if (to < 0 || to >= self.total) return;

    ui_backend_oplist_move(from, to);
    s_reorder_op = to;

    self.scroll_guard = true;
    if (to < self.visible_start)
        update_visible_range(to);
    else if (to >= self.visible_start + UI_OPLIST_VISIBLE_SLOTS)
        update_visible_range(to - UI_OPLIST_VISIBLE_SLOTS + 1);
    else
        update_visible_range(self.visible_start); // 窗口不变但数组顺序变了，仍需重刷内容
    reorder_focus_picked();
    self.scroll_guard = false;
}

static void slot_click_cb(lv_event_t *e)
{
    oplist_slot_t *s = (oplist_slot_t *)lv_event_get_user_data(e);
    if (s->op_index < 0) return;
    if (s_reorder_active) {           // 排序模式下短按确定 = 落位提交
        reorder_exit(true);
        return;
    }
    lv_obj_remove_state(s->btn, LV_STATE_PRESSED);
    ui_backend_oplist_select(s->op_index);
    screen_show(SCREEN_SPINNER);
}

static void slot_longpress_cb(lv_event_t *e)
{
    oplist_slot_t *s = (oplist_slot_t *)lv_event_get_user_data(e);
    if (s->op_index < 0 || s_reorder_active) return;
    reorder_enter(s->op_index);
}

static void slot_focus_cb(lv_event_t *e)
{
    if (self.scroll_guard) return;
    oplist_slot_t *s = (oplist_slot_t *)lv_event_get_user_data(e);
    int slot_idx = (int)(s - self.slots);
    if (s->op_index < 0) return;

    // 焦点贴近顶/底且越界则滑窗 ±1，滑后保持焦点在同一干员。
    if (slot_idx <= 1 && self.visible_start > 0) {
        self.scroll_guard = true;
        update_visible_range(self.visible_start - 1);
        refocus_op(s->op_index);
        self.scroll_guard = false;
    } else if (slot_idx >= UI_OPLIST_VISIBLE_SLOTS - 2 &&
               self.visible_start + UI_OPLIST_VISIBLE_SLOTS < self.total) {
        self.scroll_guard = true;
        update_visible_range(self.visible_start + 1);
        refocus_op(s->op_index);
        self.scroll_guard = false;
    }

    // 焦点入可见区自己瞬移(不带 LVGL 默认的平滑滚动动画)：弱端逐帧重绘那段太卡。
    lv_group_t *g = screens_group();
    lv_obj_t *foc = g ? lv_group_get_focused(g) : NULL;
    if (foc) lv_obj_scroll_to_view_recursive(foc, LV_ANIM_OFF);
}

static void make_slot(int i)
{
    oplist_slot_t *s = &self.slots[i];

    s->cont = lv_obj_create(self.scroll);
    lv_obj_set_size(s->cont, lv_pct(97), S(UI_OPLIST_ITEM_HEIGHT));
    lv_obj_set_style_pad_all(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s->cont, LV_OBJ_FLAG_SCROLLABLE);
    add_style_op_entry(s->cont);

    s->btn = lv_button_create(s->cont);
    lv_obj_set_size(s->btn, lv_pct(100), lv_pct(100));
    add_style_op_btn(s->btn);
    // 选中改到 SHORT_CLICKED(松开触发)，给长按让路：长按=进排序模式，短按=选中/落位
    lv_obj_add_event_cb(s->btn, slot_click_cb, LV_EVENT_SHORT_CLICKED, s);
    lv_obj_add_event_cb(s->btn, slot_longpress_cb, LV_EVENT_LONG_PRESSED, s);
    lv_obj_add_event_cb(s->btn, slot_focus_cb, LV_EVENT_FOCUSED, s);
    lv_obj_remove_flag(s->btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS); // 关默认动画滚动，改由 focus cb 无动画滚

    // 排序模式"拎起"高亮：被拎的槽打 USER_1 —— 整块换醒目橙底，
    // 与普通(灰底)/焦点(青底)明显拉开，一眼看出该项正处于"可上下移动"状态。
    lv_obj_set_style_bg_color(s->btn, lv_color_hex(0xF07000), LV_PART_MAIN | LV_STATE_USER_1);
    lv_obj_set_style_bg_opa(s->btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1);

    s->logo = lv_image_create(s->btn);
    lv_obj_set_pos(s->logo, 0, 0);
    lv_obj_set_size(s->logo, S(64), S(64));
    lv_image_set_inner_align(s->logo, LV_IMAGE_ALIGN_STRETCH);

    s->name = lv_label_create(s->btn);
    lv_obj_set_pos(s->name, S(68), 0);
    lv_obj_set_size(s->name, S(232), S(32)); // LONG_DOT 要固定高度才会截断，否则退化成换行
    lv_label_set_long_mode(s->name, LV_LABEL_LONG_DOT);
    add_style_label_large(s->name);

    s->desc = lv_label_create(s->btn);
    lv_obj_set_pos(s->desc, S(68), S(32));
    lv_obj_set_size(s->desc, S(213), S(32));
    lv_label_set_long_mode(s->desc, LV_LABEL_LONG_DOT);
    add_style_label_small(s->desc);

    // res 在上、sd 在下，竖直堆叠。角标加了上下 padding 变高，两者需拉开间距避免重叠。
    s->sd = lv_label_create(s->btn);
    lv_obj_set_pos(s->sd, S(281), S(52));
    add_style_sd_flag(s->sd);
    lv_label_set_text(s->sd, "数据");

    s->res = lv_label_create(s->btn);
    lv_obj_set_pos(s->res, S(281), S(26));
    add_style_res_flag(s->res);

    s->op_index = -1;
}

static void update_slot_content(int i, int op_idx)
{
    oplist_slot_t *s = &self.slots[i];
    ui_op_entry_t e;
    if (!ui_backend_oplist_get(op_idx, &e)) return;
    lv_label_set_text(s->name, e.name);
    lv_label_set_text(s->desc, e.desc);
    if (e.logo_path) lv_image_set_src(s->logo, e.logo_path);
    if (e.sd) lv_obj_remove_flag(s->sd, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(s->sd, LV_OBJ_FLAG_HIDDEN);
    if (e.res) {
        lv_label_set_text(s->res, e.res);
        lv_obj_remove_flag(s->res, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s->res, LV_OBJ_FLAG_HIDDEN);
    }
    s->op_index = op_idx;
}

static void update_visible_range(int new_start)
{
    if (new_start < 0) new_start = 0;
    int max_start = self.total - UI_OPLIST_VISIBLE_SLOTS;
    if (max_start < 0) max_start = 0;
    if (new_start > max_start) new_start = max_start;
    self.visible_start = new_start;

    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++) {
        int op_idx = new_start + i;
        if (op_idx < self.total) {
            update_slot_content(i, op_idx);
            lv_obj_remove_flag(self.slots[i].cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(self.slots[i].cont, LV_OBJ_FLAG_HIDDEN);
            self.slots[i].op_index = -1;
        }
    }
}

// 重建导航 group：仅可见槽 + 底部按钮，关 wrap，聚焦 focus_op 对应干员。
static void build_group(int focus_op)
{
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);
    lv_group_set_editing(g, false);

    if (self.total > 0 &&
        (focus_op < self.visible_start || focus_op >= self.visible_start + UI_OPLIST_VISIBLE_SLOTS)) {
        update_visible_range(focus_op);
    }

    for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++)
        if (self.slots[i].op_index >= 0) lv_group_add_obj(g, self.slots[i].btn);
    lv_group_add_obj(g, self.refresh_btn);
    lv_group_add_obj(g, self.menu_btn);
    add_style_focus(self.refresh_btn);
    add_style_focus(self.menu_btn);

    refocus_op(focus_op);
}

// 进屏时重建导航 group，聚焦当前干员。
static void on_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    if (!screens_group()) return;
    s_reorder_active = false;  // 进屏一律非排序态
    s_reorder_op = -1;
    build_group(ui_backend_oplist_current());
}

lv_obj_t *screen_oplist_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root_bare();
    ui_header(root, "干员列表");

    self.total = ui_backend_oplist_count();

    if (self.total == 0) {
        lv_obj_t *empty = lv_label_create(root);
        lv_obj_set_pos(empty, S(60), S(150));
        add_style_label_large(empty);
        lv_label_set_text(empty, "暂无干员素材\n请放入 /assets");
    } else {
        self.scroll = lv_obj_create(root);
        lv_obj_set_pos(self.scroll, S(14), S(40));
        lv_obj_set_size(self.scroll, S(332), S(280));
        lv_obj_set_flex_flow(self.scroll, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(self.scroll, LV_DIR_VER);
        lv_obj_set_style_pad_all(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        // 主题 card 默认给了按 DPI 算的 pad_row(不走 S()，两档同像素)，会盖过条目自己的
        // margin_top，导致间距不随分辨率缩放。清零，间距全由 op_entry 的 margin_top(S(5)) 定。
        lv_obj_set_style_pad_row(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        for (int i = 0; i < UI_OPLIST_VISIBLE_SLOTS; i++) make_slot(i);
        update_visible_range(0);
    }

    self.refresh_btn = ui_text_button(root, 17, 327, 159, 51, UI_SEM_SUCCESS, "刷新列表", on_refresh);
    self.menu_btn    = ui_text_button(root, 187, 327, 157, 51, UI_SEM_DEFAULT, "主菜单", on_menu);

    lv_obj_add_event_cb(root, on_load_start, LV_EVENT_SCREEN_LOAD_START, NULL);
    return root;
}
