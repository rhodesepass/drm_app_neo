#pragma once
#include <prts/prts.h>
#include <utils/uuid.h>
#include "apps/apps_types.h"

int apps_init(apps_t *apps,prts_t *prts,bool use_sd);
int apps_destroy(apps_t *apps);

int apps_try_launch_by_file(apps_t *apps,const char* working_dir,const char *basename);
int apps_try_launch_by_index(apps_t *apps,int index);
int apps_toggle_bg_app_by_index(apps_t *apps,int index);
