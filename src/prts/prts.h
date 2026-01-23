#pragma once
#include "config.h"
#include "stdint.h"
#include "utils/uuid.h"
#include "overlay/opinfo.h"
#include "overlay/transitions.h"
#include <overlay/overlay.h>
#include <stdio.h>
#include <utils/settings.h>
#include "utils/spsc_queue.h"
#include "vars.h"

typedef enum {
    PARSE_LOG_ERROR = 0,
    PARSE_LOG_WARN = 1,
} prts_parse_log_type_t;

typedef enum {
    DISPLAY_360_640 = 0,
    DISPLAY_480_854 = 1,
    DISPLAY_720_1280 = 2,
} display_type_t;

// 干员来源
typedef enum {
    OP_SOURCE_NAND = 0,
    OP_SOURCE_SD
} op_source_t;

typedef struct {
    char path[128];

    // only valid in intro:
    bool enabled;
    int duration;
} prts_video_t;

typedef struct {
    int index;
    char operator_name[40];
    uuid_t uuid;
    char description[256];
    char icon_path[128];
    display_type_t disp_type;

    prts_video_t intro_video;
    prts_video_t loop_video;

    olopinfo_params_t opinfo_params;
    oltr_params_t transition_in;
    oltr_params_t transition_loop;

    op_source_t source;  // 干员来源 (NAND/SD)
} prts_operator_entry_t;



typedef enum {
    PRTS_REQUEST_NONE = 0,
    // 请求切换到指定干员
    PRTS_REQUEST_SET_OPERATOR,
} prts_request_type_t;


typedef struct {
    prts_request_type_t type;
    int operator_index;
    // 请求处理结束后 需不需要释放
    bool on_heap;
} prts_request_t;

typedef enum {
    PRTS_STATE_IDLE = 0, // 正在播放loop动画。
    PRTS_STATE_TRANSITION_IN, // 入场过渡
    PRTS_STATE_INTRO, //入场视频
    PRTS_STATE_TRANSITION_LOOP, // 循环过渡
    PRTS_STATE_PRE_OPINFO, // 显示干员信息前的等待
} prts_state_t;

typedef struct {
    overlay_t * overlay;

    // prts 当前状态
    prts_state_t state;

    prts_operator_entry_t operators[PRTS_OPERATORS_MAX];
    int operator_count;
    int operator_index;

    FILE* parse_log_f;

    // 上次发生干员切换的时机
    uint64_t last_switch_time;
    prts_timer_handle_t timer_handle;

    spsc_bq_t req_queue;
} prts_t;

void prts_init(prts_t* prts,overlay_t* overlay,bool use_sd);
void prts_destroy(prts_t* prts);

void prts_log_parse_log(prts_t* prts,char* path,char* message,prts_parse_log_type_t type);
void prts_request_set_operator(prts_t* prts,int operator_index);