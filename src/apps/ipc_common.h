// IPC 请求格式
#pragma once
#include <stdint.h>
#include <utils/uuid.h>
#include "config.h"
#include "utils/settings.h"
#include "prts/prts.h"

// =====================================
// 结构定义
// =====================================

// ========== UI 子模块 ==========
// 弹窗 - 请求数据。
typedef struct {
    char title[UI_WARNING_MAX_TITLE_LENGTH];
    char desc[UI_WARNING_MAX_DESC_LENGTH];
    char icon[UI_WARNING_MAX_ICON_LENGTH];
    uint32_t color;
} ipc_req_ui_warning_data_t;
// 弹窗 - 响应数据（空）

// 获取当前所在界面 - 请求数据（空）
// 获取当前所在界面 - 响应数据。
typedef struct {
    curr_screen_t screen;
} ipc_resp_ui_get_current_screen_data_t;

// 设置当前所在界面 - 请求数据。
typedef struct {
    curr_screen_t screen;
} ipc_req_ui_set_current_screen_data_t;
// 设置当前所在界面 - 响应数据（空）

// 将扩列图强制设定为给定的路径 - 请求数据。
typedef struct {
    char path[128];
} ipc_req_ui_force_dispimg_data_t;
// 将扩列图强制设定为给定的路径 - 响应数据（空）

// ========== PRTS 子模块 ==========

// 获取PRTS状态 - 请求数据（空）
// 获取PRTS状态 - 响应数据。
typedef struct {
    prts_state_t state;
    int operator_count;
    int operator_index;
} ipc_resp_prts_status_data_t;

// 请求切换干员 - 请求数据。
typedef struct {
    int operator_index;
} ipc_req_prts_set_operator_data_t;
// 请求切换干员 - 响应数据（空）

// 获取干员信息 - 请求数据。
typedef struct {
    int operator_index;
} ipc_req_prts_get_operator_info_data_t;
// 获取干员信息 - 响应数据。
typedef struct {
    int operator_index;
    char operator_name[40];
    uuid_t uuid;
    char description[256];
    char icon_path[128];
    prts_source_t source;
} ipc_prts_operator_info_data_t;

// 设置闭锁普瑞塞斯自动切换（时间切换） - 请求数据。
typedef struct {
    bool is_blocked;
} ipc_req_prts_set_blocked_auto_switch_data_t;
// 设置闭锁普瑞塞斯自动切换（时间切换） - 响应数据（空）

// 重新从磁盘扫描并加载干员素材 - 请求数据（空）
// 重新从磁盘扫描并加载干员素材 - 响应数据（空）

// ========== Settings 子模块 ==========
// 获取设置 - 请求数据（空）
// 整定设置 - 响应数据（空）

// 整定设置 - 请求数据
// 获取设置 - 响应数据。
typedef struct {
    int brightness;
    sw_interval_t switch_interval;
    sw_mode_t switch_mode;
    usb_mode_t usb_mode;
    settings_ctrl_word_t ctrl_word;
} ipc_settings_data_t;

// ========== MediaPlayer 子模块 ==========
// 获取当前播放的视频路径 - 请求数据（空）
// 设置当前播放的视频路径 - 响应数据。（空）
// 获取当前播放的视频路径 - 响应数据。
// 设置当前播放的视频路径 - 请求数据。

typedef struct {
    char path[128];
} ipc_mediaplayer_video_path_data_t;


// ========== Overlay 子模块 ==========
// 进行一次过渡排期 - 请求数据。
typedef struct {
    int duration;
    transition_type_t type;
    char image_path[128];
    uint32_t background_color;
} ipc_req_overlay_schedule_transition_data_t;
// 进行一次过渡排期 - 响应数据。（空）

// 进行一次带过渡的视频更换排期 - 请求数据。
typedef struct {
    char video_path[128];
    int duration;
    transition_type_t type;
    char image_path[128];
    uint32_t background_color;
} ipc_req_overlay_schedule_transition_video_data_t;
// 进行一次带过渡的视频更换排期 - 响应数据。（空）

// 全局 部分
// 退出应用 - 请求数据。
typedef struct{
    int exit_code;
} ipc_req_app_exit_data_t;
// 退出应用 - 响应数据（空）

// ========== UIX 子模块（外部交互会话）==========
// 轮询式：START 立即返回 session_id，结果由客户端 SESSION_POLL 轮询。
// handler 不等用户操作（IPC 线程不能阻塞，UI 结果产生在 LVGL 线程）。
// 同时只允许一个会话，冲突回 IPC_RESP_ERROR_STATE_CONFLICT。
// 类型定义在独立小头（sim 的 UI 代码也要编，不能拖进 prts 等设备侧依赖）。
#include <ui/uix_types.h>

// =====================================
// 请求 部分
// =====================================
typedef enum {
    // UI 子模块
    // 弹窗。
    IPC_REQ_UI_WARNING = 0,
    // 获取当前所在界面。
    IPC_REQ_UI_GET_CURRENT_SCREEN = 1,
    // 设置当前所在界面。
    IPC_REQ_UI_SET_CURRENT_SCREEN = 2,
    // 将扩列图强制设定为给定的路径。不校验。
    IPC_REQ_UI_FORCE_DISPIMG = 3,
    
    // PRTS 子模块
    // 获取PRTS状态。
    IPC_REQ_PRTS_GET_STATUS = 4,
    // 请求切换干员。
    IPC_REQ_PRTS_SET_OPERATOR = 5,
    // 获取干员信息。
    IPC_REQ_PRTS_GET_OPERATOR_INFO = 6,
    // 设置闭锁普瑞塞斯自动切换（时间切换）
    // 类似设置里的
    IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH = 7,
    // 重新从磁盘扫描并加载干员素材。
    IPC_REQ_PRTS_RELOAD_ASSETS = 15,
    
    // Settings 子模块
    // 获取设置。
    IPC_REQ_SETTINGS_GET = 8,
    // 整定设置并落盘。
    IPC_REQ_SETTINGS_SET = 9,

    // mediaplayer 子模块
    // 获取当前播放的视频路径。
    IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH = 10,
    // 设置当前播放的视频路径。(直接设置并播放)
    IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH = 11,

    // overlay 子模块
    // 进行一次纯过渡排期（不涉及视频更换）。
    IPC_REQ_OVERLAY_SCHEDULE_TRANSITION = 12,
    // 进行一次带过渡的视频更换排期
    IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO = 13,

    // 全局请求
    IPC_REQ_APP_EXIT = 14,

    // UIX 子模块（外部交互会话，usb_aio_handler 用）
    IPC_REQ_UIX_CONFIRM_START = 16,
    IPC_REQ_UIX_USB_SELECT_START = 17,
    IPC_REQ_UIX_SESSION_POLL = 18,
    IPC_REQ_UIX_SESSION_CANCEL = 19,

    // app 子模块：重新从磁盘扫描并刷新应用列表。
    IPC_REQ_APPS_RELOAD_LIST = 20,

    IPC_REQ_MAX = 21,
} ipc_req_type_t;

typedef struct {
    ipc_req_type_t type;
    union {
        ipc_req_ui_warning_data_t ui_warning;
        ipc_req_ui_set_current_screen_data_t ui_set_current_screen;
        ipc_req_ui_force_dispimg_data_t ui_force_dispimg;
        ipc_req_prts_set_operator_data_t prts_set_operator;
        ipc_req_prts_get_operator_info_data_t prts_get_operator;
        ipc_req_prts_set_blocked_auto_switch_data_t prts_set_blocked_auto_switch;
        ipc_settings_data_t settings;
        ipc_mediaplayer_video_path_data_t mediaplayer_video_path;
        ipc_req_overlay_schedule_transition_data_t overlay_schedule_transition;
        ipc_req_overlay_schedule_transition_video_data_t overlay_schedule_transition_video;
        ipc_req_app_exit_data_t app_exit;
        ipc_req_uix_confirm_start_data_t uix_confirm_start;
        ipc_req_uix_usb_select_start_data_t uix_usb_select_start;
        ipc_req_uix_session_data_t uix_session;
    };
} ipc_req_t;


// =====================================
// 响应 部分
// =====================================

typedef enum {
    IPC_RESP_OK = 0,
    IPC_RESP_ERROR_MSG_TOO_LONG = 1,
    IPC_RESP_ERROR_NOMEM = 2,
    IPC_RESP_ERROR_INVALID_REQUEST = 3,
    IPC_RESP_ERROR_STATE_CONFLICT = 4,
    IPC_RESP_ERROR_LENGTH_MISMATCH = 5,
    IPC_RESP_ERROR_UNKNOWN = 6,
} ipc_resp_type_t;

typedef struct {
    ipc_resp_type_t type;
    union {
        ipc_resp_ui_get_current_screen_data_t ui_current_screen;
        ipc_resp_prts_status_data_t prts_status;
        ipc_prts_operator_info_data_t prts_operator_info;
        ipc_settings_data_t settings;
        ipc_mediaplayer_video_path_data_t mediaplayer_video_path;
        ipc_resp_uix_session_start_data_t uix_session_start;
        ipc_resp_uix_session_poll_data_t uix_session_poll;
    };
} ipc_resp_t;

const static ipc_resp_t ipc_resp_too_large = {
    .type = IPC_RESP_ERROR_MSG_TOO_LONG,
};

size_t calculate_ipc_req_size(ipc_req_type_t type);
size_t calculate_ipc_resp_size_by_req(ipc_req_type_t type);

#ifndef APP_RELEASE
void ipc_print_req_type(ipc_req_type_t type);
void ipc_print_res_type(ipc_resp_type_t type);
#else
#define ipc_print_req_type(type) do {} while(0)
#define ipc_print_res_type(type) do {} while(0)
#endif