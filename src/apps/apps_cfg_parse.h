#pragma once
#include "apps/apps.h"

int apps_cfg_try_load(apps_t *apps,app_entry_t* app,char * path,app_source_t source,int index);
int apps_cfg_scan(apps_t *apps,char* dirpath,app_source_t source);

#ifndef APP_RELEASE
void apps_cfg_log_entry(app_entry_t* app);
#else
#define apps_cfg_log_entry(app) do {} while(0)
#endif // APP_RELEASE