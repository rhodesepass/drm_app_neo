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

// 前向定义
static void schedule_video_and_transitions(prts_t* prts,prts_video_t* video,oltr_params_t* transition,bool is_first_transition);

typedef struct {
    prts_t* prts;
    prts_video_t* video;
    oltr_params_t* transition;
    bool is_first_transition;
    // 需不需要释放这个结构体
    bool on_heap;
} schedule_video_and_transitions_timer_data_t;

// intro视频播放完（达到intro的duration之后）触发的定时器回调。调度transition_loop -> loop video
static void schedule_video_and_transitions_timer_cb(void* userdata,bool is_last){
    schedule_video_and_transitions_timer_data_t* data = (schedule_video_and_transitions_timer_data_t*)userdata;
    schedule_video_and_transitions(data->prts, data->video, data->transition, data->is_first_transition);
    data->prts->state = PRTS_STATE_TRANSITION_LOOP;
    if(data->on_heap){
        free(data);
    }
}

static void schedule_opinfo_timer_cb(void* userdata,bool is_last){
    prts_t* prts = (prts_t*)userdata;
    prts_operator_entry_t* target_operator = &prts->operators[prts->operator_index];
    if(target_operator->opinfo_params.type == OPINFO_TYPE_ARKNIGHTS){
        overlay_opinfo_show_arknights(prts->overlay, &target_operator->opinfo_params);
    }
    else if(target_operator->opinfo_params.type == OPINFO_TYPE_IMAGE){
        overlay_opinfo_show_image(prts->overlay, &target_operator->opinfo_params);
    }
    else{
        log_error("schedule_opinfo_timer_cb: invalid opinfo type: %d", target_operator->opinfo_params.type);
    }
    prts->state = PRTS_STATE_IDLE;
}

static void schedule_opinfo(prts_t* prts,prts_operator_entry_t* target_operator){
    if(target_operator->opinfo_params.type != OPINFO_TYPE_NONE){
        prts_timer_handle_t timer_handle;
        prts_timer_create(&timer_handle, 
            target_operator->opinfo_params.appear_time, 
            0, 
            1, 
            schedule_opinfo_timer_cb, 
            (void*)prts);
    }
}

// 让我们捋一下干员切换的时间线。
// ===上一态loop video== | =transition_in== | =intro video== | =transition_loop== | =====OPINFO +loop video ==
//               干员切换|   d*1|  d*2|  d*3|                |   d*1|   d*2|   d*3| appear_time|
//             middle_cb切视频-|            |       middle_cb切视频-|             |
//                            |     end_cb-|                       |      end_cb-|
//                            | <======Intro Video实际播放时长 ===> |
// 
// 所以，我们在end_cb 调用的时候 ,排期transition_loop的时间应该是 
// intro_video.duration - transition_in.duration * 2 - transtion_loop.duration

static void schedule_video_and_transitions_end_cb(void* userdata,bool is_last){
    log_trace("schedule_video_and_transitions_end_cb");
    prts_t* prts = (prts_t*)userdata;
    prts_operator_entry_t* target_operator = &prts->operators[prts->operator_index];

    prts_state_t curr_state = prts->state;
    prts_state_t next_state = PRTS_STATE_IDLE;

    
    if(curr_state == PRTS_STATE_TRANSITION_IN){
        // 入场过渡结束，进入intro视频。等intro视频结束后，排期transition_loop -> loop video
        schedule_video_and_transitions_timer_data_t *data = malloc(sizeof(schedule_video_and_transitions_timer_data_t));
        data->prts = prts;
        data->video = &target_operator->loop_video;
        data->transition = &target_operator->transition_loop;
        data->is_first_transition = false;
        data->on_heap = true;
        int delay = target_operator->intro_video.duration - target_operator->transition_in.duration * 2 - target_operator->transition_loop.duration;
        if(delay < 0){
            log_error("schedule_video_and_transitions_end_cb: delay < 0, delay: %d", delay);
            delay = 100 * 1000;
        }
        prts_timer_handle_t timer_handle;
        prts_timer_create(&timer_handle, delay, 0, 1, schedule_video_and_transitions_timer_cb, (void*)data);
        next_state = PRTS_STATE_INTRO;
    }
    else if(prts->state == PRTS_STATE_TRANSITION_LOOP){
        // 排期opinfo
        if(target_operator->opinfo_params.type != OPINFO_TYPE_NONE && g_settings.ctrl_word.no_overlay_block == 0){
            schedule_opinfo(prts, target_operator);
            next_state = PRTS_STATE_PRE_OPINFO;
        }
        else{
            next_state = PRTS_STATE_IDLE;
        }
    }
    else{
        log_error("schedule_video_and_transitions_end_cb: invalid state: %d", curr_state);
    }

    prts->state = next_state;
}

static void schedule_video_and_transitions(prts_t* prts,prts_video_t* video,oltr_params_t* transition,bool is_first_transition){
    oltr_callback_t* callback = malloc(sizeof(oltr_callback_t));

    log_trace("schedule_video_and_transitions: video: %s, transition: %d, is_first_transition: %d", video->path, transition->type, is_first_transition);
    
    callback->middle_cb_userdata = video;
    callback->end_cb = schedule_video_and_transitions_end_cb;
    callback->end_cb_userdata = prts;
    callback->on_heap = true;

    // 第一次发生过渡时 有两个问题:
    // 1. 需要挂载视频图层（用mount_video_layer_callback）
    // 2. 不能使用fade过渡，否则有bug（强制用swipe）
    if(is_first_transition){
        callback->middle_cb = set_video_mount_layer_cb;
        overlay_transition_swipe(prts->overlay, callback, transition);
    }
    else{
        callback->middle_cb = set_video_cb;
        switch(transition->type){
            case TRANSITION_TYPE_FADE:
                overlay_transition_fade(prts->overlay, callback, transition);
                break;
            case TRANSITION_TYPE_MOVE:
                overlay_transition_move(prts->overlay, callback, transition);
                break;
            case TRANSITION_TYPE_SWIPE:
                overlay_transition_swipe(prts->overlay, callback, transition);
                break;
            default:
                log_error("invalid transition type: %d", transition->type);
                break;
        }
    }
}


typedef struct {
    prts_t* prts;
    int target_index;
    prts_operator_entry_t* target_operator;
    bool *is_first_switch;
    bool on_heap;
} switch_operator_secound_stage_data_t;

static void switch_operator_secound_stage(void* userdata,bool is_last){
    switch_operator_secound_stage_data_t* data = (switch_operator_secound_stage_data_t*)userdata;
    prts_t* prts = data->prts;
    int target_index = data->target_index;
    prts_operator_entry_t* target_operator = data->target_operator;
    bool is_first_switch = *data->is_first_switch;


    prts_state_t next_state = PRTS_STATE_IDLE;

    // 第一步。 存在intro video，且闭锁入场动画软压板没投
    // 则做全量 transition_in -> intro video -> transition_loop -> loop video
    if(target_operator->intro_video.enabled && g_settings.ctrl_word.no_intro_block == 0){
        schedule_video_and_transitions(prts, 
            &target_operator->intro_video, 
            &target_operator->transition_in, 
            is_first_switch
        );
        next_state = PRTS_STATE_TRANSITION_IN;
    }
    // 不存在 intro video，则做 transition_in -> loop video
    // 我格式没设计好，所以明明使用的是transition_in 却进入了LOOP状态
    // 这个LOOP状态用于在回调中推进状态机。
    else{
        schedule_video_and_transitions(prts, 
            &target_operator->loop_video, 
            &target_operator->transition_in,
            is_first_switch
        );
        next_state = PRTS_STATE_TRANSITION_LOOP;
    }

    prts->last_switch_time = get_now_us();
    prts->operator_index = target_index;
    *data->is_first_switch = false;
    prts->state = next_state;

    if(data->on_heap){
        free(data);
    }

}

static void switch_operator(prts_t* prts,int target_index){
    static bool is_first_switch = true;

    prts_operator_entry_t* target_operator = &prts->operators[target_index];
    prts_operator_entry_t* curr_operator = &prts->operators[prts->operator_index];


    if(prts->state != PRTS_STATE_IDLE){
        log_error("switch_operator: prts is not idle?? curr_state: %d", prts->state);
        return;
    }

    log_info("switching operator from %s to %s", curr_operator->operator_name, target_operator->operator_name);

    // 卸载当前干员。此时overlay播放消失动画，但是mediaplayer还在运行。
    if(!is_first_switch){
        overlay_abort(prts->overlay);
        overlay_opinfo_free_image(&curr_operator->opinfo_params);
        overlay_transition_free_image(&curr_operator->transition_in);
        overlay_transition_free_image(&curr_operator->transition_loop);
    }

    switch_operator_secound_stage_data_t *data = malloc(sizeof(switch_operator_secound_stage_data_t));
    data->prts = prts;
    data->target_index = target_index;
    data->target_operator = target_operator;
    data->is_first_switch = &is_first_switch;
    data->on_heap = true;

    // 先排期 overlay_abort结束后的操作（第二阶段）
    // 一旦调用第二阶段的schedule代码 就会立刻把buffer覆盖。
    // 我们要等overlay先结束之后 再进入第二阶段。
    prts_timer_handle_t timer_handle;
    prts_timer_create(
        &timer_handle, 
        UI_LAYER_ANIMATION_DURATION, 
        0, 
        1, 
        switch_operator_secound_stage, 
        (void*)data
    );
    
    // 加载新干员
    overlay_transition_load_image(&target_operator->transition_in);
    overlay_transition_load_image(&target_operator->transition_loop);
    overlay_opinfo_load_image(&target_operator->opinfo_params);

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
        if(prts->state != PRTS_STATE_IDLE){
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

    // 由 时间触发
    if(target_operator_index == -1){
        if(!ui_is_hidden()){
            log_warn("switch_operator: ui is not hidden, skip switch");
            settings_unlock(&g_settings);
            return;
        }
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


void prts_init(prts_t* prts, overlay_t* overlay, bool use_sd){
    log_info("==> PRTS Initializing...");
    prts->overlay = overlay;
    prts->parse_log_f = fopen(PRTS_OPERATOR_PARSE_LOG, "w");
    if(prts->parse_log_f == NULL){
        log_error("failed to open parse log file: %s", PRTS_OPERATOR_PARSE_LOG);
    }
    prts->operator_count = 0;

    int errcnt = prts_operator_scan_assets(prts, PRTS_ASSET_DIR, OP_SOURCE_NAND);

    if(use_sd){
        log_info("==> PRTS will scan SD assets directory: %s", PRTS_ASSET_DIR_SD);
        errcnt += prts_operator_scan_assets(prts, PRTS_ASSET_DIR_SD, OP_SOURCE_SD);
    }

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
        prts_operator_try_load(prts, &prts->operators[0], PRTS_FALLBACK_ASSET_DIR, OP_SOURCE_NAND);
        prts->operator_count = 1;
    }

    for(int i = 0; i < prts->operator_count; i++){
        prts->operators[i].index = i;
#ifndef APP_RELEASE
        log_debug("========================");
        log_debug("operator[%d]:", i);
        prts_operator_log_entry(&prts->operators[i]);
#endif // APP_RELEASE
    }

    spsc_bq_init(&prts->req_queue, 10);

    log_info("==> PRTS will perform first switch...");
    // 进行第一次干员切换
    prts->state = PRTS_STATE_IDLE;
    prts->operator_index = 0;
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