#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void create_applist(void);

void add_applist_to_group(void);

void refresh_applist(void);

void destroy_applist(void);

#ifdef __cplusplus
}
#endif
