#include "ui/filemanager.h"
#include "ui.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "utils/log.h"

extern objects_t objects;

extern groups_t groups;
extern int g_running;
extern int g_exitcode;

#ifndef EXITCODE_APPSTART
#define EXITCODE_APPSTART 2
#endif

static const char * strip_lv_fs_prefix(const char * path)
{
    if(!path) return path;
    if(isalpha((unsigned char)path[0]) && path[1] == ':') return path + 2;
    return path;
}

static void file_explorer_event_handler(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t * fe = lv_event_get_target(e);
    const char * cur_path = lv_file_explorer_get_current_path(fe);
    const char * sel_fn = lv_file_explorer_get_selected_file_name(fe);

    if(!cur_path || !sel_fn || sel_fn[0] == '\0') return;

    char full_path[512];
    lv_snprintf(full_path, sizeof(full_path), "%s%s", cur_path, sel_fn);

    const char * abs_path = strip_lv_fs_prefix(full_path);

    FILE * fp = fopen("/tmp/appstart", "w");
    if(!fp) {
        log_error("unable to write to /tmp/appstart???: %s", strerror(errno));
        return;
    }

    fprintf(fp, "chmod +x %s\n", abs_path);
    fprintf(fp, "%s\n", abs_path);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    log_info("/tmp/appstart written: %s", abs_path);
    g_exitcode = EXITCODE_APPSTART;
    g_running = 0;
}

static lv_obj_t * fe;
void create_filemanager(){
    lv_obj_t *root = objects.file_container;
    if(!root) {
        return;
    }

    lv_obj_clean(root);

    lv_obj_set_style_bg_color(root, lv_color_hex(0xf2f1f6), 0);

    fe = lv_file_explorer_create(root);
    lv_obj_set_style_bg_color(fe, lv_color_hex(0xf2f1f6), 0);


    lv_obj_set_size(fe, LV_PCT(100), LV_PCT(100));
    lv_obj_center(fe);

    lv_file_explorer_open_dir(fe, "A:/root/");

    lv_obj_add_event_cb(fe, file_explorer_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * file_table = lv_file_explorer_get_file_table(fe);
    if(groups.op && file_table) {
        lv_group_add_obj(groups.op, file_table);
        lv_group_focus_obj(file_table);
    }
}

void add_filemanager_to_group(){
    lv_obj_t * file_table = lv_file_explorer_get_file_table(fe);
    if(groups.op && file_table) {
        lv_group_add_obj(groups.op, file_table);
        lv_group_focus_obj(file_table);
    }
}