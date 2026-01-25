#pragma once
#include "utils/uuid.h"
#include "utils/timer.h"
#include "config.h"
#include <stdio.h>


typedef enum {
    APP_SOURCE_NAND = 0,
    APP_SOURCE_SD = 1,
} app_source_t;

typedef enum {
    APP_TYPE_FOREGROUND = 0,
    // 只可以根据拓展名从文件模块中启动。
    APP_TYPE_FOREGROUND_EXTENSION_ONLY = 1,
    APP_TYPE_BACKGROUND = 2,
} app_type_t;

typedef struct {
    int index;
    char app_name[40];
    uuid_t uuid;
    char description[256];
    char icon_path[128];
    app_source_t source;
    // executable path
    char executable_path[128];
    // also workdirectory
    char app_dir[128];
    app_type_t type;

    // background app 专属参数
    int pid;
} app_entry_t;

typedef struct {
    char ext[10];
    app_entry_t *app;
} apps_extmap_entry_t;

typedef struct {
    apps_extmap_entry_t extmap[APPS_EXTMAP_MAX];
    int extmap_count;
} apps_extmap_t;



typedef struct {
    app_entry_t apps[APPS_MAX];
    int app_count;

    apps_extmap_t extmap;

    // 维护后台应用的timer
    prts_timer_handle_t bg_app_check_timer;

    FILE* parse_log_f;
} apps_t;