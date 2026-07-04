#include "screen_applist.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"

// 应用列表：焦点驱动虚拟滚动 (同 oplist)。移植自原 actions_apps.c。
typedef struct {
    lv_obj_t *cont;
    lv_obj_t *btn;
    lv_obj_t *logo;
    lv_obj_t *name;
    lv_obj_t *desc;
    lv_obj_t *state; // 前/后台角标
    lv_obj_t *sd;
    int       app_index;
    int       last_state; // 已应用的角标状态，-1=未设；去重避免每帧叠 style
} applist_slot_t;

static struct {
    lv_obj_t      *scroll;
    lv_obj_t      *back_btn;
    applist_slot_t slots[UI_APP_VISIBLE_SLOTS];
    int            total;
    int            visible_start;
    bool           scroll_guard;
} self;

static void update_visible_range(int new_start);

static void on_back(lv_event_t *e) { (void)e; screen_show(SCREEN_MAINMENU); }

static void slot_click_cb(lv_event_t *e)
{
    applist_slot_t *s = (applist_slot_t *)lv_event_get_user_data(e);
    if (s->app_index < 0) return;
    lv_obj_remove_state(s->btn, LV_STATE_PRESSED);
    ui_backend_applist_select(s->app_index);
    screen_show(SCREEN_SPINNER);
}

static void refocus_app(int app_index)
{
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++)
        if (self.slots[i].app_index == app_index) { lv_group_focus_obj(self.slots[i].btn); return; }
}

static void slot_focus_cb(lv_event_t *e)
{
    if (self.scroll_guard) return;
    applist_slot_t *s = (applist_slot_t *)lv_event_get_user_data(e);
    int slot_idx = (int)(s - self.slots);
    if (s->app_index < 0) return;

    if (slot_idx <= 1 && self.visible_start > 0) {
        self.scroll_guard = true;
        update_visible_range(self.visible_start - 1);
        refocus_app(s->app_index);
        self.scroll_guard = false;
    } else if (slot_idx >= UI_APP_VISIBLE_SLOTS - 2 &&
               self.visible_start + UI_APP_VISIBLE_SLOTS < self.total) {
        self.scroll_guard = true;
        update_visible_range(self.visible_start + 1);
        refocus_app(s->app_index);
        self.scroll_guard = false;
    }
}

static void make_slot(int i)
{
    applist_slot_t *s = &self.slots[i];

    s->cont = lv_obj_create(self.scroll);
    lv_obj_set_size(s->cont, lv_pct(97), S(UI_APP_ITEM_HEIGHT));
    lv_obj_set_style_pad_all(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s->cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s->cont, LV_OBJ_FLAG_SCROLLABLE);
    add_style_op_entry(s->cont);

    s->btn = lv_button_create(s->cont);
    lv_obj_set_size(s->btn, lv_pct(100), lv_pct(100));
    add_style_op_btn(s->btn);
    lv_obj_add_event_cb(s->btn, slot_click_cb, LV_EVENT_PRESSED, s);
    lv_obj_add_event_cb(s->btn, slot_focus_cb, LV_EVENT_FOCUSED, s);

    s->logo = lv_image_create(s->btn);
    lv_obj_set_pos(s->logo, 0, 0);
    lv_obj_set_size(s->logo, S(64), S(64));
    lv_image_set_inner_align(s->logo, LV_IMAGE_ALIGN_STRETCH);

    s->name = lv_label_create(s->btn);
    lv_obj_set_pos(s->name, S(68), 0);
    lv_obj_set_width(s->name, S(266));
    lv_label_set_long_mode(s->name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    add_style_label_large(s->name);

    s->desc = lv_label_create(s->btn);
    lv_obj_set_pos(s->desc, S(68), S(32));
    lv_obj_set_width(s->desc, S(245));
    add_style_label_small(s->desc);

    s->state = lv_label_create(s->btn);
    lv_obj_set_pos(s->state, S(303), S(47));

    s->sd = lv_label_create(s->btn);
    lv_obj_set_pos(s->sd, S(313), S(30));
    add_style_sd_flag(s->sd);
    lv_label_set_text(s->sd, "SD");

    s->app_index = -1;
    s->last_state = -1;
}

// 仅刷新某槽的前/后台角标 (tick 复用 + 内容更新共用)。state 未变则跳过 (避免每帧叠 style)。
static void apply_state_flag(applist_slot_t *s, ui_app_state_t st)
{
    if ((int)st == s->last_state) return;
    s->last_state = (int)st;
    if (st == UI_APP_FG) {
        add_style_app_fg(s->state);
        lv_label_set_text(s->state, "前台");
        lv_obj_remove_flag(s->state, LV_OBJ_FLAG_HIDDEN);
    } else if (st == UI_APP_BG) {
        add_style_app_bg_running(s->state);
        lv_label_set_text(s->state, "后台");
        lv_obj_remove_flag(s->state, LV_OBJ_FLAG_HIDDEN);
    } else { // STOPPED 后台未运行
        add_style_app_bg_notrunning(s->state);
        lv_label_set_text(s->state, "后台");
        lv_obj_remove_flag(s->state, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_slot_content(int i, int app_idx)
{
    applist_slot_t *s = &self.slots[i];
    ui_app_entry_t e;
    if (!ui_backend_applist_get(app_idx, &e)) return;
    lv_label_set_text(s->name, e.name);
    lv_label_set_text(s->desc, e.desc);
    if (e.logo_path) lv_image_set_src(s->logo, e.logo_path);
    apply_state_flag(s, e.state);
    if (e.sd) lv_obj_remove_flag(s->sd, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(s->sd, LV_OBJ_FLAG_HIDDEN);
    s->app_index = app_idx;
}

static void update_visible_range(int new_start)
{
    if (new_start < 0) new_start = 0;
    int max_start = self.total - UI_APP_VISIBLE_SLOTS;
    if (max_start < 0) max_start = 0;
    if (new_start > max_start) new_start = max_start;
    self.visible_start = new_start;

    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        int idx = new_start + i;
        if (idx < self.total) {
            update_slot_content(i, idx);
            lv_obj_remove_flag(self.slots[i].cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(self.slots[i].cont, LV_OBJ_FLAG_HIDDEN);
            self.slots[i].app_index = -1;
        }
    }
}

// 周期刷新可见槽的后台运行状态 (后台 app 起停后角标变色)。
void screen_applist_tick(void)
{
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) {
        int idx = self.slots[i].app_index;
        if (idx < 0) continue;
        ui_app_entry_t e;
        if (ui_backend_applist_get(idx, &e)) apply_state_flag(&self.slots[i], e.state);
    }
}

static void on_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);
    for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++)
        if (self.slots[i].app_index >= 0) lv_group_add_obj(g, self.slots[i].btn);
    lv_group_add_obj(g, self.back_btn);
    add_style_focus(self.back_btn);
    if (self.total > 0) lv_group_focus_obj(self.slots[0].btn);
}

lv_obj_t *screen_applist_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root_bare();
    ui_header(root, "应用列表");

    self.total = ui_backend_applist_count();

    if (self.total == 0) {
        lv_obj_t *empty = lv_label_create(root);
        lv_obj_set_pos(empty, S(67), S(203));
        add_style_label_large(empty);
        lv_label_set_text(empty, "设备内没有应用程序\n请将程序安装到\n/app");
    } else {
        self.scroll = lv_obj_create(root);
        lv_obj_set_pos(self.scroll, 0, S(40));
        lv_obj_set_size(self.scroll, S(360), S(520));
        lv_obj_set_flex_flow(self.scroll, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(self.scroll, LV_DIR_VER);
        lv_obj_set_style_pad_all(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        // 主题 card 默认给了按 DPI 算的 pad_row(不走 S()，两档同像素)，会盖过条目自己的
        // margin_top，导致间距不随分辨率缩放。清零，间距全由 op_entry 的 margin_top(S(5)) 定。
        lv_obj_set_style_pad_row(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(self.scroll, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        for (int i = 0; i < UI_APP_VISIBLE_SLOTS; i++) make_slot(i);
        update_visible_range(0);
    }

    self.back_btn = ui_text_button(root, 23, 574, 316, 51, UI_SEM_DEFAULT, "返回", on_back);

    lv_obj_add_event_cb(root, on_load_start, LV_EVENT_SCREEN_LOAD_START, NULL);
    return root;
}
