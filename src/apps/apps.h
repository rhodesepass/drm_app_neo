#pragma once
#include <prts/prts.h>
#include <utils/uuid.h>
#include "apps/apps_types.h"

int apps_init(apps_t *apps,prts_t *prts);
int apps_destroy(apps_t *apps);

// 从磁盘重新扫描并刷新应用列表(SD 热插拔,无条件 NAND+SD 两边都扫)。
// 运行中的后台应用 pid 按 uuid 保留。必须在读取该列表的 LVGL 线程内调用
// (见 ui_backend_reload_applist)。返回扫描错误数。
int apps_reload(apps_t *apps);

int apps_try_launch_by_file(apps_t *apps,const char* working_dir,const char *basename);
int apps_try_launch_by_index(apps_t *apps,int index);
int apps_toggle_bg_app_by_index(apps_t *apps,int index);
