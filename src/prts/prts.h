#pragma once
#include "config.h"
#include "stdint.h"
#include "utils/uuid.h"
#include "overlay/opinfo.h"
#include "overlay/transitions.h"

typedef enum {
    DISPLAY_360_640 = 0,
    DISPLAY_480_854 = 1,
    DISPLAY_720_1280 = 2,
} display_type_t;

typedef struct {
    char path[128];

    // only valid in intro:
    bool enabled;
    int duration;
} prts_video_t;

typedef struct {
    char operator_name[40];
    uuid_t uuid;
    char description[256];
    char icon_path[128];
    display_type_t disp_type;

    prts_video_t intro_video;
    prts_video_t loop_video;

    olopinfo_params_t opinfo_params;
    oltr_params_t transition_params;

} prts_operator_entry_t;

typedef struct {
    int exitcode;
} prts_t;

prts_t g_prts;

void prts_init();
void prts_destroy();
