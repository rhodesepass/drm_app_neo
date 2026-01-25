#pragma once
#include "apps/apps_types.h"


int apps_extmap_init(apps_extmap_t *extmap);
int apps_extmap_destroy(apps_extmap_t *extmap);
int apps_extmap_add(apps_extmap_t *extmap, char *ext, app_entry_t *app);
int apps_extmap_get(apps_extmap_t *extmap, char *ext, app_entry_t **app);

#ifndef APP_RELEASE
void apps_extmap_log_entry(apps_extmap_t *extmap);
#else
#define apps_extmap_log_entry(extmap) do {} while(0)
#endif // APP_RELEASE