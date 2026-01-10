#include "prts/prts.h"
#include <dirent.h>
#include <overlay/opinfo.h>
#include <overlay/overlay.h>
#include <overlay/transitions.h>
#include <prts/operators.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <ui/actions_warning.h>
#include <utils/timer.h>
#include <fcntl.h>
#include "utils/log.h"
#include <unistd.h>
#include <stdlib.h>
#include "config.h"
#include "utils/settings.h"
#include "vars.h"
#include "render/mediaplayer.h"
#include "ui/scr_transition.h"

extern settings_t g_settings;
extern uint64_t get_now_us(void);
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
    ui_warning((warning_type_t)userdata);
}


inline static bool should_switch_by_interval(prts_t* prts){
    uint64_t interval_us = 0;

    switch(g_settings.switch_interval){
        case sw_interval_t_SW_INTERVAL_1MIN:
            interval_us = 1 * 60 * 1000 * 1000;
            break;
        case sw_interval_t_SW_INTERVAL_3MIN:
            interval_us = 3 * 60 * 1000 * 1000;
            break;
        case sw_interval_t_SW_INTERVAL_5MIN:
            interval_us = 5 * 60 * 1000 * 1000;
            break;
        case sw_interval_t_SW_INTERVAL_10MIN:
            interval_us = 10 * 60 * 1000 * 1000;
            break;
        case sw_interval_t_SW_INTERVAL_30MIN:
            interval_us = 30 * 60 * 1000 * 1000;
            break;
        default:
            log_error("invalid switch interval: %d", g_settings.switch_interval);
            return false;
    }


    if(g_settings.switch_mode == sw_mode_t_SW_RANDOM_MANUAL){
        return false;
    }
    else{
        return get_now_us() - prts->last_switch_time > interval_us;
    }
}

inline static int get_switch_target_index(prts_t* prts){

    if(prts->operator_count == 1){
        return 0;
    }

    int target_index = -1;


    if(g_settings.switch_mode == sw_mode_t_SW_MODE_SEQUENCE){
        return (prts->operator_index + 1) % prts->operator_count;
    }
    else if(g_settings.switch_mode == sw_mode_t_SW_MODE_RANDOM){
        do{
            target_index = rand() % prts->operator_count;
        }while(target_index == prts->operator_index);
        return target_index;
    }
    else{
        log_error("invalid switch mode: %d", g_settings.switch_mode);
        return prts->operator_index;
    }
}


extern mediaplayer_t g_mediaplayer;

typedef struct {
    overlay_t* overlay;
    oltr_params_t* transition;
    prts_video_t* video;
    bool is_first_switch;
} start_transition_data_t;

static void set_video_cb(void* userdata,bool is_last){
    log_trace("set_video_cb");
    prts_video_t* data = (prts_video_t*)userdata;
    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_play_video(&g_mediaplayer, data->path);
}

extern void mount_video_layer_callback(void *userdata,bool is_last);
static void set_video_mount_layer_cb(void* userdata,bool is_last){
    log_trace("set_video_mount_layer_cb");
    prts_video_t* data = (prts_video_t*)userdata;
    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_play_video(&g_mediaplayer, data->path);

    mount_video_layer_callback(userdata, is_last);
}

static void start_transition_cb(void* userdata,bool is_last){
    start_transition_data_t* data = (start_transition_data_t*)userdata;
    void (*cb)(void* userdata,bool is_last) = NULL;
    // fixme: 第一次过渡时 如果效果是fade会出现透明度问题
    // 这里直接写死用swipe。
    if(data->is_first_switch){
        cb = set_video_mount_layer_cb;
        if(data->transition->type != TRANSITION_TYPE_NONE){
            overlay_transition_swipe(data->overlay, cb, data->video, data->transition);
        }
    }
    else{
        cb = set_video_cb;
        switch(data->transition->type){
            case TRANSITION_TYPE_FADE:
                overlay_transition_fade(data->overlay, cb, data->video, data->transition);
                break;
            case TRANSITION_TYPE_MOVE:
                overlay_transition_move(data->overlay, cb, data->video, data->transition);
                break;
            case TRANSITION_TYPE_SWIPE:
                overlay_transition_swipe(data->overlay, cb, data->video, data->transition);
                break;
            default:
                break;
        }
    }
    // log_trace("start_transition_cb");
    // log_trace("transition->type: %d", data->transition->type);
    // log_trace("transition->duration: %d", data->transition->duration);
    // log_trace("transition->image_path: %s", data->transition->image_path);
    // log_trace("transition->image_w: %d", data->transition->image_w);
    // log_trace("transition->image_h: %d", data->transition->image_h);
    // log_trace("transition->image_addr: %p", data->transition->image_addr);
    // log_trace("transition->background_color: %x", data->transition->background_color);

    if(is_last){
        free(data);
    }
}

// 排期 视频+过渡 
// return:  这个过渡执行完所需要的时间。
static uint64_t schedule_video_and_transitions(prts_t* prts,prts_video_t* video,oltr_params_t* transition,uint64_t delay_us,bool is_first_switch){
    start_transition_data_t *data = (start_transition_data_t*)malloc(sizeof(start_transition_data_t));
    data->overlay = prts->overlay;
    data->transition = transition;
    data->video = video;
    data->is_first_switch = is_first_switch;

    if(delay_us == 0){
        prts_timer_create(
            &prts->timer_handle, 
            100,
            0,
            1,
            start_transition_cb,
            data
        );
    }
    else{
        prts_timer_create(
            &prts->timer_handle, 
            delay_us,
            0,
            1,
            start_transition_cb,
            data
        );
    }

    if(transition->type == TRANSITION_TYPE_NONE){
        return 0;
    }
    else{
        return 3 * transition->duration;
    }
}

typedef struct {
    overlay_t* overlay;
    olopinfo_params_t* opinfo;
} start_opinfo_data_t;


static void start_opinfo_cb(void* userdata,bool is_last){
    start_opinfo_data_t* data = (start_opinfo_data_t*)userdata;
    switch(data->opinfo->type){
        case OPINFO_TYPE_IMAGE:
            overlay_opinfo_show_image(data->overlay, data->opinfo);
            break;
        case OPINFO_TYPE_ARKNIGHTS:
            overlay_opinfo_show_arknights(data->overlay, data->opinfo);
            break;
        default:
            break;
    }
    if(is_last){
        free(data);
    }
}
// 排期 opinfo
static uint64_t schedule_opinfo(prts_t* prts,olopinfo_params_t* opinfo,uint64_t delay_us){
    // log_info("schedule_opinfo");
    // log_info("delay_us: %llu", delay_us);
    // log_info("opinfo->appear_time: %d", opinfo->appear_time);
    // log_info("opinfo->duration: %d", opinfo->duration);
    // log_info("opinfo->type: %d", opinfo->type);
    // log_info("opinfo->operator_name: %s", opinfo->operator_name);
    // log_info("opinfo->operator_code: %s", opinfo->operator_code);
    // log_info("opinfo->barcode_text: %s", opinfo->barcode_text);
    // log_info("opinfo->aux_text: %s", opinfo->aux_text);
    // log_info("opinfo->staff_text: %s", opinfo->staff_text);
    // log_info("opinfo->color: %x", opinfo->color);
    // log_info("opinfo->logo_path: %s", opinfo->logo_path);
    // log_info("opinfo->class_path: %s", opinfo->class_path);
    // log_info("opinfo->logo_w: %d", opinfo->logo_w);
    // log_info("opinfo->logo_h: %d", opinfo->logo_h);
    // log_info("opinfo->class_w: %d", opinfo->class_w);
    // log_info("opinfo->class_h: %d", opinfo->class_h);
    // log_info("opinfo->logo_addr: %p", opinfo->logo_addr);
    // log_info("opinfo->class_addr: %p", opinfo->class_addr);
    // log_info("opinfo->logo_addr: %p", opinfo->logo_addr);
    start_opinfo_data_t *data = (start_opinfo_data_t*)malloc(sizeof(start_opinfo_data_t));
    data->overlay = prts->overlay;
    data->opinfo = opinfo;

    // FIXME: 加1s的延迟，防止opinfo和transition_in/loop同时开始.
    // 这个其实是transition跳帧导致的。最好的办法是在transtion里加end cb.

    prts_timer_create(
        &prts->timer_handle, 
        delay_us + opinfo->appear_time + 1 * 1000 * 1000,
        0,
        1,
        start_opinfo_cb,
        data
    );
    switch(opinfo->type){
        case OPINFO_TYPE_IMAGE:
            return opinfo->appear_time + opinfo->duration;
        case OPINFO_TYPE_ARKNIGHTS:
            return opinfo->appear_time + OVERLAY_ANIMATION_OPINFO_ARKNIGHTS_DURATION;
        default:
            return 0;
    }
}

static void clear_prts_busy_cb(void* userdata,bool is_last){
    prts_t* prts = (prts_t*)userdata;
    prts->is_busy = false;
}



static void switch_operator(prts_t* prts,int target_index){
    static bool is_first_switch = true;

    prts_operator_entry_t* target_operator = &prts->operators[target_index];
    prts_operator_entry_t* curr_operator = &prts->operators[prts->operator_index];

    log_info("switching operator from %s to %s", curr_operator->operator_name, target_operator->operator_name);

    // 卸载当前干员。此时overlay播放消失动画，但是mediaplayer还在运行。
    if(!is_first_switch){
        overlay_abort(prts->overlay);
        overlay_opinfo_free_image(&curr_operator->opinfo_params);
        overlay_transition_free_image(&curr_operator->transition_in);
        overlay_transition_free_image(&curr_operator->transition_loop);
        ui_top_fix();
    }

    // 加载新干员
    overlay_transition_load_image(&target_operator->transition_in);
    overlay_transition_load_image(&target_operator->transition_loop);
    overlay_opinfo_load_image(&target_operator->opinfo_params);

    // 给UI消失的时间。
    uint64_t total_delay_us = UI_LAYER_ANIMATION_DURATION;

    // 存在intro video，且闭锁入场动画软压板没投
    // 则做全量 transition_in -> intro video -> transition_loop -> loop video
    if(target_operator->intro_video.enabled && g_settings.ctrl_word.no_intro_block == 0){
        total_delay_us += schedule_video_and_transitions(prts, 
            &target_operator->intro_video, 
            &target_operator->transition_in, 
            total_delay_us,
            is_first_switch
        );

        total_delay_us += schedule_video_and_transitions(prts, 
            &target_operator->loop_video, 
            &target_operator->transition_loop, 
            total_delay_us + target_operator->intro_video.duration,
            false
        );
        total_delay_us += target_operator->intro_video.duration;

    }
    // 不存在 intro video，则做 transition_in -> loop video
    else{
        total_delay_us += schedule_video_and_transitions(prts, 
            &target_operator->loop_video, 
            &target_operator->transition_in, 
            total_delay_us,
            is_first_switch
        );
    }

    log_info("total delay us: %llu", total_delay_us);

    // 排期 opinfo
    if(target_operator->opinfo_params.type != OPINFO_TYPE_NONE && g_settings.ctrl_word.no_overlay_block == 0){
        total_delay_us += schedule_opinfo(prts, 
            &target_operator->opinfo_params, 
            total_delay_us
        );
    }
    
    // 在总延时结束之后 清空is_busy
    prts_timer_create(
        &prts->timer_handle, 
        total_delay_us,
        0,
        1,
        clear_prts_busy_cb,
        prts
    );

    prts->last_switch_time = get_now_us();
    prts->operator_index = target_index;
    is_first_switch = false;
}



static void prts_tick_cb(void* userdata,bool is_last){
    prts_t* prts = (prts_t*)userdata;
    prts_request_t* req;


    // 如果 这一次tick中 需要处理多个干员切换，我们只处理最后一次
    // 以防止切换堆叠到一起的情况。
    int target_operator_index = -1;

    // 处理所有的对普瑞塞斯的请求。
    while(spsc_bq_try_pop(&prts->req_queue, (void**)&req) == 0){
        switch(req->type){
            case PRTS_REQUEST_SET_OPERATOR:
                target_operator_index = req->operator_index;
                break;
            default:
                log_error("invalid request type: %d", req->type);
                break;
        }

        if(req->on_heap){
            free(req);
        }
    }

    settings_lock(&g_settings);
    bool interval_sw = should_switch_by_interval(prts);
    // 应当发生干员切换
    if(target_operator_index != -1 || interval_sw){
        // 如果PRTS正在处理干员切换，则告警
        if(prts->is_busy){
            ui_warning(UI_WARNING_PRTS_CONFLICT);
            log_warn("prts is busy, skip switch");
            settings_unlock(&g_settings);
            return;
        }
    }
    else{
        settings_unlock(&g_settings);
        return;
    }

    prts->is_busy = true;

    // 由 外部对PRTS发起切换请求。
    if(target_operator_index == -1){
        target_operator_index = get_switch_target_index(prts);
    }

    switch_operator(prts, target_operator_index);

    settings_unlock(&g_settings);

    return;
}


void prts_request_set_operator(prts_t* prts,int operator_index){
    prts_request_t* req = malloc(sizeof(prts_request_t));
    req->type = PRTS_REQUEST_SET_OPERATOR;
    req->operator_index = operator_index;
    req->on_heap = true;
    spsc_bq_push(&prts->req_queue, (void *)req);
}


void prts_init(prts_t* prts, overlay_t* overlay){

    log_info("==> PRTS Initializing...");
    prts->overlay = overlay;
    prts->parse_log_f = fopen(PRTS_OPERATOR_PARSE_LOG, "w");
    if(prts->parse_log_f == NULL){
        log_error("failed to open parse log file: %s", PRTS_OPERATOR_PARSE_LOG);
    }
    prts->operator_count = 0;

    int errcnt = prts_operator_scan_assets(prts, PRTS_ASSET_DIR);
    if(errcnt != 0){
        // 告警信号要等UI启动后才能发送，这里塞到定时器回调里
        prts_timer_handle_t warning_handle;
        prts_timer_create(&warning_handle, 
            5 * 1000 * 1000, 
            0, 
            1, 
            delayed_warning_cb, 
            (void *)UI_WARNING_ASSET_ERROR
        );
        log_warn("failed to load assets, error count: %d", errcnt);
    }

    if(prts->operator_count == 0){
        log_warn("no assets loaded, using fallback");
        prts_timer_handle_t warning_handle;
        prts_timer_create(&warning_handle, 
            5 * 1000 * 1000, 
            0, 
            1, 
            delayed_warning_cb, 
            (void *)UI_WARNING_NO_ASSETS
        );
        prts_operator_try_load(prts, &prts->operators[0], PRTS_FALLBACK_ASSET_DIR);
        prts->operator_count = 1;
    }

    for(int i = 0; i < prts->operator_count; i++){
        prts->operators[i].index = i;
        log_debug("========================");
        log_debug("operator[%d]:", i);
        prts_operator_log_entry(&prts->operators[i]);
    }

    spsc_bq_init(&prts->req_queue, 10);

    log_info("==> PRTS will perform first switch...");
    // 进行第一次干员切换
    prts->is_busy = true;
    switch_operator(prts, 0);
    
    prts_timer_create(
        &prts->timer_handle, 
        0,
        PRTS_TICK_PERIOD, 
        -1, 
        prts_tick_cb, 
        prts
    );

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