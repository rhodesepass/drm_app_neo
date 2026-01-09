#include "prts/prts.h"
#include <dirent.h>
#include <prts/operators.h>
#include <stdio.h>
#include <ui/actions_warning.h>
#include <utils/timer.h>
#include <fcntl.h>
#include "utils/log.h"
#include <unistd.h>
#include "utils/cJSON.h"
#include <string.h>
#include <stdlib.h>

void prts_log_parse_log(prts_t* prts,char* path,char* message,prts_parse_log_type_t type){
    if(prts->parse_log_f == NULL){
        return;
    }
    switch(type){
        case PARSE_LOG_ERROR:
            fprintf(prts->parse_log_f, "在处理%s时发生错误: %s\n", path, message);
            log_error("在处理%s时发生错误: %s", path, message);
            break;
        case PARSE_LOG_WARN:
            fprintf(prts->parse_log_f, "在处理%s时发生警告: %s\n", path, message);
            log_warn("在处理%s时发生警告: %s", path, message);
            break;
    }
    fflush(prts->parse_log_f);
}

static void delayed_warning_cb(void* userdata,bool is_last){
    ui_warning(UI_WARNING_ASSET_ERROR);
}


void prts_tick(prts_t* prts){
    
    return;
}

void prts_init(prts_t* prts){

    prts->parse_log_f = fopen(PRTS_OPERATOR_PARSE_LOG, "w");
    if(prts->parse_log_f == NULL){
        log_error("failed to open parse log file: %s", PRTS_OPERATOR_PARSE_LOG);
    }
    prts->operator_count = 0;

    int errcnt = prts_operator_scan_assets(prts, PRTS_ASSET_DIR);
    if(errcnt != 0){
        // 告警信号要等UI启动后才能发送，这里塞到定时器回调里
        prts_timer_handle_t handle;
        prts_timer_create(&handle, 
            3 * 1000 * 1000, 
            0, 
            1, 
            delayed_warning_cb, 
            NULL
        );
        log_warn("failed to load assets, error count: %d", errcnt);
    }

    for(int i = 0; i < prts->operator_count; i++){
        log_debug("========================");
        log_debug("operator[%d]:", i);
        prts_operator_log_entry(&prts->operators[i]);
    }

    log_info("==> PRTS Initalized!");

}
void prts_destroy(prts_t* prts){
    if(prts->parse_log_f != NULL){
        fclose(prts->parse_log_f);
    }
    if(prts->timer_handle){
        prts_timer_cancel(prts->timer_handle);
    }
}