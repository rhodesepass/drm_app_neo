#include "apps.h"
#include "utils/log.h"
#include "utils/timer.h"
#include <apps/apps_cfg_parse.h>
#include <apps/extmap.h>
#include <config.h>
#include <signal.h>
#include <ui/actions_warning.h>

static void apps_bg_app_check_timer_cb(void *userdata, bool is_last){
    apps_t *apps = (apps_t *)userdata;
    for(int i = 0; i < apps->app_count; i++){
        if(apps->apps[i].type == APP_TYPE_BACKGROUND){
            if(apps->apps[i].pid != -1){
                // if pid still alive
                if(kill(apps->apps[i].pid, 0) == 0){
                    continue;
                }
                else{
                    // pid is not alive
                    apps->apps[i].pid = -1;
                }
            }
        }
    }
}

int apps_init(apps_t *apps,bool use_sd){
    apps->app_count = 0;

    apps->parse_log_f = fopen(APPS_PARSE_LOG, "w");
    if(apps->parse_log_f == NULL){
        log_error("failed to open parse log file: %s", APPS_PARSE_LOG);
    }

    int errcnt = apps_cfg_scan(apps, APPS_DIR,APP_SOURCE_NAND);

    if(use_sd){
        log_info("==> Apps will scan SD directory: %s", APPS_DIR_SD);
        errcnt += apps_cfg_scan(apps, APPS_DIR_SD,APP_SOURCE_SD);
    }

    if(errcnt != 0){
        log_warn("failed to load apps, error count: %d", errcnt);
        ui_warning(UI_WARNING_APP_LOAD_ERROR);
    }

#ifndef APP_RELEASE
    for(int i = 0; i < apps->app_count; i++){
        apps_cfg_log_entry(&apps->apps[i]);
    }
    apps_extmap_log_entry(&apps->extmap);
#endif // APP_RELEASE

    prts_timer_create(
        &apps->bg_app_check_timer, 
        0, APPS_BG_APP_CHECK_PERIOD, -1, 
        apps_bg_app_check_timer_cb, apps);
    return 0;
}

int apps_destroy(apps_t *apps){
    apps->app_count = 0;
    fclose(apps->parse_log_f);
    return 0;
}

