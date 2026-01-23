#include "ui/applist.h"
#include "ui.h"
#include "apps/apps.h"
#include "apps/apps_launcher.h"
#include "ui/scr_transition.h"
#include "utils/log.h"
#include "config.h"
#include "styles.h"
#include "images.h"

#include <string.h>

// ========== 外部对象 ==========

extern objects_t objects;
extern groups_t groups;
extern bool g_use_sd;

// ========== 布局常量 (参考 EEZ app_list user widget) ==========

#define APP_ITEM_HEIGHT     80      // 列表项高度
#define APP_ICON_SIZE_EEZ   64      // 图标尺寸
#define APP_LABEL_X         68      // 标签X偏移
#define APP_LABEL_WIDTH     232     // 标签宽度
#define APP_LABEL_HEIGHT    32      // 标签高度
#define APP_DESC_Y          32      // 描述标签Y偏移

// ========== 内部状态 ==========

static apps_manager_t g_apps_mgr;
static lv_obj_t* g_scroll_container = NULL;
static bool g_initialized = false;

// ========== 事件处理 ==========

/**
 * @brief 应用点击回调
 */
static void app_click_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_entry_t* app = apps_get(&g_apps_mgr, idx);
    if (!app) {
        log_error("[APPLIST] Invalid app index: %d", idx);
        return;
    }

    log_info("[APPLIST] Selected app: %s (index=%d)", app->name, idx);

    // 启动应用
    if (app_launch(&g_apps_mgr, idx) != 0) {
        log_error("[APPLIST] Failed to launch app: %s", app->name);
        // TODO: 显示错误提示
    }

    // 启动完成后会自动返回，刷新列表
    refresh_applist();
}

// ========== 辅助函数 ==========

static lv_obj_t* create_app_item(lv_obj_t* parent, app_entry_t* app, int index) {
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_size(item, LV_PCT(97), APP_ITEM_HEIGHT);
    lv_obj_set_style_pad_left(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
    add_style_op_entry(item);

    lv_obj_t* btn = lv_button_create(item);
    lv_obj_set_pos(btn, 0, 0);
    lv_obj_set_size(btn, LV_PCT(100), LV_PCT(100));
    add_style_op_btn(btn);
    lv_obj_add_event_cb(btn, app_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);

    lv_obj_t* icon = lv_image_create(btn);
    lv_obj_set_pos(icon, 0, 0);
    lv_obj_set_size(icon, APP_ICON_SIZE_EEZ, APP_ICON_SIZE_EEZ);
    if (app->icon_path[0] != '\0') {
        lv_image_set_src(icon, app->icon_path);
    } else {
        lv_image_set_src(icon, &img_prts);  // 默认图标
    }
    lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_STRETCH);

    lv_obj_t* name_label = lv_label_create(btn);
    lv_obj_set_pos(name_label, APP_LABEL_X, 0);
    lv_obj_set_size(name_label, APP_LABEL_WIDTH, APP_LABEL_HEIGHT);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    add_style_label_large(name_label);
    lv_label_set_text(name_label, app->name);

    lv_obj_t* desc_label = lv_label_create(btn);
    lv_obj_set_pos(desc_label, APP_LABEL_X, APP_DESC_Y);
    lv_obj_set_size(desc_label, APP_LABEL_WIDTH, APP_LABEL_HEIGHT);
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_DOT);
    add_style_label_small(desc_label);
    lv_obj_set_style_text_line_space(desc_label, -1, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 只显示描述（来源通过右上角标记显示）
    if (app->description[0] != '\0') {
        lv_label_set_text(desc_label, app->description);
    } else {
        lv_label_set_text(desc_label, "");
    }

    // SD卡来源标记 (右上角)
    if (app->source == APP_SOURCE_SD) {
        lv_obj_t* sd_label = lv_label_create(btn);
        lv_label_set_text(sd_label, "SD");
        lv_obj_align(sd_label, LV_ALIGN_TOP_RIGHT, -4, 4);  // 右上角，边距4px
        lv_obj_set_style_text_color(sd_label, lv_color_hex(0x88ff88), 0);  // 绿色
        lv_obj_set_style_text_font(sd_label, &lv_font_montserrat_14, 0);   // 小字体
    }

    return btn;  // 返回按钮用于添加到导航组
}

static void create_empty_hint(lv_obj_t* parent) {
    // 提示容器
    lv_obj_t* hint_container = lv_obj_create(parent);
    lv_obj_set_size(hint_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hint_container, 0, 0);
    lv_obj_set_style_border_width(hint_container, 0, 0);
    lv_obj_set_style_pad_all(hint_container, 20, 0);
    lv_obj_set_flex_flow(hint_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hint_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 标题
    lv_obj_t* title = lv_label_create(hint_container);
    add_style_label_large(title);
    lv_label_set_text(title, "没有找到应用");
    lv_obj_set_style_text_color(title, lv_color_hex(0xaaaaaa), 0);

    // 提示1
    lv_obj_t* hint1 = lv_label_create(hint_container);
    add_style_label_small(hint1);
    lv_label_set_text(hint1, "请将应用放入以下目录:");
    lv_obj_set_style_text_color(hint1, lv_color_hex(0x888888), 0);

    // 路径1
    lv_obj_t* path1 = lv_label_create(hint_container);
    add_style_label_small(path1);
    lv_label_set_text(path1, APPS_DIR_NAND);
    lv_obj_set_style_text_color(path1, lv_color_hex(0x6688aa), 0);

    // 路径2 (SD卡)
    if (g_use_sd) {
        lv_obj_t* path2 = lv_label_create(hint_container);
        add_style_label_small(path2);
        lv_label_set_text(path2, APPS_DIR_SD);
        lv_obj_set_style_text_color(path2, lv_color_hex(0x6688aa), 0);
    }
}

// ========== 公共接口 ==========

void create_applist(void) {
    lv_obj_t* root = objects.app_container;
    if (!root) {
        log_error("[APPLIST] app_container not found!");
        return;
    }

    log_info("[APPLIST] Creating app list with EEZ styling...");

    // 清空容器
    lv_obj_clean(root);

    // 初始化应用管理器（如果还没有）
    if (!g_initialized) {
        apps_manager_init(&g_apps_mgr);
        g_initialized = true;
    }

    // 扫描应用
    apps_scan(&g_apps_mgr, g_use_sd);

    g_scroll_container = lv_obj_create(root);
    lv_obj_set_size(g_scroll_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(g_scroll_container, 0, 0);
    lv_obj_set_style_border_width(g_scroll_container, 0, 0);
    lv_obj_set_style_pad_all(g_scroll_container, 0, 0);
    lv_obj_set_flex_flow(g_scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(g_scroll_container, 8, 0);  // 列表项间距
    lv_obj_set_scroll_dir(g_scroll_container, LV_DIR_VER);
    lv_obj_add_flag(g_scroll_container, LV_OBJ_FLAG_SCROLLABLE);

    // 添加应用项
    int app_count = apps_get_count(&g_apps_mgr);
    log_info("[APPLIST] Found %d apps", app_count);

    for (int i = 0; i < app_count; i++) {
        app_entry_t* app = apps_get(&g_apps_mgr, i);
        if (!app) continue;

        create_app_item(g_scroll_container, app, i);
    }

    // 如果没有应用，显示提示
    if (app_count == 0) {
        create_empty_hint(g_scroll_container);
    }

    log_info("[APPLIST] App list created with EEZ styling");
}

void add_applist_to_group(void) {
    if (!g_scroll_container || !groups.op) return;

    log_info("[APPLIST] Adding to navigation group...");

    lv_group_remove_all_objs(groups.op);

    // 遍历滚动容器中的所有子对象，找到按钮并添加到组
    uint32_t item_cnt = lv_obj_get_child_count(g_scroll_container);
    for (uint32_t i = 0; i < item_cnt; i++) {
        lv_obj_t* item = lv_obj_get_child(g_scroll_container, i);
        // 每个 item 是容器，按钮是其第一个子对象
        lv_obj_t* btn = lv_obj_get_child(item, 0);
        if (btn && lv_obj_check_type(btn, &lv_button_class)) {
            lv_group_add_obj(groups.op, btn);
        }
    }

    // 添加返回按钮（如果存在）
    if (objects.applist_back_btn) {
        lv_group_add_obj(groups.op, objects.applist_back_btn);
    }

    // 聚焦第一个按钮
    if (lv_group_get_obj_count(groups.op) > 0) {
        lv_group_focus_next(groups.op);
    }
}

void refresh_applist(void) {
    log_info("[APPLIST] Refreshing app list...");
    create_applist();
    add_applist_to_group();
}

void destroy_applist(void) {
    if (g_initialized) {
        apps_manager_destroy(&g_apps_mgr);
        g_initialized = false;
    }

    g_scroll_container = NULL;
}
