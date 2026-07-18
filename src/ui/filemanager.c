#include "ui/filemanager.h"
#include "ui_screens/screen_manager.h" // ui_hook_filemanager_*

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <lvgl/lvgl.h>

#include "config.h"
#include "utils/log.h"
#include "apps/apps.h"
#include "ui/ui_theme.h"

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

// lv_fs 路径 (带盘符) 目录能否打开：SD 拔了/目录被删时探测用
static bool fm_dir_openable(const char *lvpath)
{
    lv_fs_dir_t d;
    if (lv_fs_dir_open(&d, lvpath) != LV_FS_RES_OK) return false;
    lv_fs_dir_close(&d);
    return true;
}

// 把当前浏览目录存到普通文件 (posix 路径)，内容是 lv_fs 带盘符路径
static void fm_save_last_dir(void)
{
    if (!s_fe) return;
    const char *cur = lv_file_explorer_get_current_path(s_fe);
    if (!cur || cur[0] == '\0') return;
    FILE *f = fopen(FILEMANAGER_LAST_DIR_FILE, "w");
    if (!f) { log_warn("filemanager: cannot write last dir"); return; }
    fputs(cur, f);
    fclose(f);
}

// 恢复上次目录；读不到/目录已不存在则退回根目录
static void fm_open_last_or_root(void)
{
    char buf[256];
    FILE *f = fopen(FILEMANAGER_LAST_DIR_FILE, "r");
    if (f) {
        char *ok = fgets(buf, sizeof(buf), f);
        fclose(f);
        if (ok) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
            if (n > 0 && fm_dir_openable(buf)) {
                lv_file_explorer_open_dir(s_fe, buf);
                return;
            }
        }
    }
    lv_file_explorer_open_dir(s_fe, FILEMANAGER_ROOT_DIR);
}

static void file_explorer_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *fe = lv_event_get_target(e);
    const char *cur_path = lv_file_explorer_get_current_path(fe);
    const char *sel_fn   = lv_file_explorer_get_selected_file_name(fe);
    if (!cur_path || !sel_fn || sel_fn[0] == '\0') return;
    if (!s_apps) { log_error("filemanager: apps is NULL"); return; }
    fm_save_last_dir(); // 选中文件即将启动 app 离开，先记住所在目录
    apps_try_launch_by_file(s_apps, strip_lv_fs_prefix(cur_path), sel_fn);
}

// 设备实现 ui_hook_filemanager_mount: 在 container 内建 lv_file_explorer。
void ui_hook_filemanager_mount(lv_obj_t *container)
{
    if (!container) return;
    lv_obj_clean(container);
    lv_obj_set_style_bg_color(container, ui_color(UI_C_SURFACE), 0);

    s_fe = lv_file_explorer_create(container);
    lv_obj_set_style_bg_color(s_fe, ui_color(UI_C_SURFACE), 0);
    lv_obj_set_size(s_fe, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_fe);
    fm_open_last_or_root();
    lv_obj_add_event_cb(s_fe, file_explorer_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // 文件不多时表体下方留白画的是 table MAIN 底色(默认白)，跟 surface 统一
    lv_obj_t *ft = lv_file_explorer_get_file_table(s_fe);
    if (ft) {
        lv_obj_set_style_bg_color(ft, ui_color(UI_C_SURFACE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ft, LV_OPA_COVER, LV_PART_MAIN);
    }
}

// 设备实现 ui_hook_filemanager_enter: 文件表加入导航 group 并聚焦。
void ui_hook_filemanager_enter(lv_group_t *group)
{
    if (!s_fe || !group) return;
    lv_obj_t *file_table = lv_file_explorer_get_file_table(s_fe);
    if (file_table) {
        lv_group_add_obj(group, file_table);
        lv_group_focus_obj(file_table);
        // encoder indev 下 lv_table 是可编辑控件，默认聚焦≠进编辑态，
        // 得先按一次 ENTER 才能用旋转翻行。这里直接进编辑态省掉那一下。
        lv_group_set_editing(group, true);
    }
}

// 设备实现 ui_hook_filemanager_leave: 离屏时把当前浏览目录存盘。
void ui_hook_filemanager_leave(void)
{
    fm_save_last_dir();
}
