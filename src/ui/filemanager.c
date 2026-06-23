#include "ui/filemanager.h"
#include "ui_screens/screen_manager.h" // ui_hook_filemanager_*

#include <ctype.h>
#include <lvgl/lvgl.h>

#include "config.h"
#include "utils/log.h"
#include "apps/apps.h"

static apps_t   *s_apps;
static lv_obj_t *s_fe;   // lv_file_explorer

void filemanager_init(apps_t *apps)
{
    s_apps = apps;
}

static const char *strip_lv_fs_prefix(const char *path)
{
    if (!path) return path;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return path + 2;
    return path;
}

static void file_explorer_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *fe = lv_event_get_target(e);
    const char *cur_path = lv_file_explorer_get_current_path(fe);
    const char *sel_fn   = lv_file_explorer_get_selected_file_name(fe);
    if (!cur_path || !sel_fn || sel_fn[0] == '\0') return;
    if (!s_apps) { log_error("filemanager: apps is NULL"); return; }
    apps_try_launch_by_file(s_apps, strip_lv_fs_prefix(cur_path), sel_fn);
}

// 设备实现 ui_hook_filemanager_mount: 在 container 内建 lv_file_explorer。
void ui_hook_filemanager_mount(lv_obj_t *container)
{
    if (!container) return;
    lv_obj_clean(container);
    lv_obj_set_style_bg_color(container, lv_color_hex(0xf2f1f6), 0);

    s_fe = lv_file_explorer_create(container);
    lv_obj_set_style_bg_color(s_fe, lv_color_hex(0xf2f1f6), 0);
    lv_obj_set_size(s_fe, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_fe);
    lv_file_explorer_open_dir(s_fe, "A:/root/");
    lv_obj_add_event_cb(s_fe, file_explorer_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
}

// 设备实现 ui_hook_filemanager_enter: 文件表加入导航 group 并聚焦。
void ui_hook_filemanager_enter(lv_group_t *group)
{
    if (!s_fe || !group) return;
    lv_obj_t *file_table = lv_file_explorer_get_file_table(s_fe);
    if (file_table) {
        lv_group_add_obj(group, file_table);
        lv_group_focus_obj(file_table);
    }
}
