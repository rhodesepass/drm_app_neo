#pragma once
#include <utils/uuid.h>
#include "apps/apps_types.h"

int apps_init(apps_t *apps,bool use_sd);
int apps_destroy(apps_t *apps);
