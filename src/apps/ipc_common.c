#include "apps/ipc_common.h"
#include <utils/log.h>


size_t calculate_ipc_req_size(ipc_req_type_t type){
    size_t size = sizeof(ipc_req_type_t);
    switch (type){
        case IPC_REQ_UI_WARNING:
            size += sizeof(ipc_req_ui_warning_data_t);
            break;
        case IPC_REQ_UI_GET_CURRENT_SCREEN:
            // No additional data
            break;
        case IPC_REQ_UI_SET_CURRENT_SCREEN:
            size += sizeof(ipc_req_ui_set_current_screen_data_t);
            break;
        case IPC_REQ_UI_FORCE_DISPIMG:
            size += sizeof(ipc_req_ui_force_dispimg_data_t);
            break;
        case IPC_REQ_PRTS_GET_STATUS:
            // No additional data
            break;
        case IPC_REQ_PRTS_SET_OPERATOR:
            size += sizeof(ipc_req_prts_set_operator_data_t);
            break;
        case IPC_REQ_PRTS_GET_OPERATOR_INFO:
            size += sizeof(ipc_req_prts_get_operator_info_data_t);
            break;
        case IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH:
            size += sizeof(ipc_req_prts_set_blocked_auto_switch_data_t);
            break;
        case IPC_REQ_SETTINGS_GET:
            // No additional data
            break;
        case IPC_REQ_SETTINGS_SET:
            size += sizeof(ipc_settings_data_t);
            break;
        case IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH:
            // No additional data
            break;
        case IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH:
            size += sizeof(ipc_mediaplayer_video_path_data_t);
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION:
            size += sizeof(ipc_req_overlay_schedule_transition_data_t);
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO:
            size += sizeof(ipc_req_overlay_schedule_transition_video_data_t);
            break;
        case IPC_REQ_APP_EXIT:
            size += sizeof(ipc_req_app_exit_data_t);
            break;
        default:
            log_error("calculate_ipc_req_size: unknown request type: %d", type);
            break;
    }
    return size;
}


size_t calculate_ipc_resp_size_by_req(ipc_req_type_t type){
    size_t size = sizeof(ipc_req_type_t);
    switch (type){
        case IPC_REQ_UI_WARNING:
            // No additional data
            break;
        case IPC_REQ_UI_GET_CURRENT_SCREEN:
            size += sizeof(ipc_resp_ui_get_current_screen_data_t);
            break;
        case IPC_REQ_UI_SET_CURRENT_SCREEN:
            // No additional data
            break;
        case IPC_REQ_UI_FORCE_DISPIMG:
            // No additional data
            break;
        case IPC_REQ_PRTS_GET_STATUS:
            size += sizeof(ipc_resp_prts_status_data_t);
            break;
        case IPC_REQ_PRTS_SET_OPERATOR:
            // No additional data
            break;
        case IPC_REQ_PRTS_GET_OPERATOR_INFO:
            // No additional data
            break;
        case IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH:
            // No additional data
            break;
        case IPC_REQ_SETTINGS_GET:
            size += sizeof(ipc_settings_data_t);
            break;
        case IPC_REQ_SETTINGS_SET:
            // No additional data
            break;
        case IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH:
            size += sizeof(ipc_mediaplayer_video_path_data_t);
            break;
        case IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH:
            // No additional data
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION:
            // No additional data
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO:
            // No additional data
            break;
        case IPC_REQ_APP_EXIT:
            // No additional data
            break;
        default:
            log_error("calculate_ipc_resp_size_by_req: unknown request type: %d", type);
            break;
    }
    return size;
}

// debug用

#ifndef APP_RELEASE
void ipc_print_req_type(ipc_req_type_t type){
    switch (type){
        case IPC_REQ_UI_WARNING:
            log_debug("IPC_REQ_UI_WARNING");
            break;
        case IPC_REQ_UI_GET_CURRENT_SCREEN:
            log_debug("IPC_REQ_UI_GET_CURRENT_SCREEN");
            break;
        case IPC_REQ_UI_SET_CURRENT_SCREEN:
            log_debug("IPC_REQ_UI_SET_CURRENT_SCREEN");
            break;
        case IPC_REQ_UI_FORCE_DISPIMG:
            log_debug("IPC_REQ_UI_FORCE_DISPIMG");
            break;
        case IPC_REQ_PRTS_GET_STATUS:
            log_debug("IPC_REQ_PRTS_GET_STATUS");
            break;
        case IPC_REQ_PRTS_SET_OPERATOR:
            log_debug("IPC_REQ_PRTS_SET_OPERATOR");
            break;
        case IPC_REQ_PRTS_GET_OPERATOR_INFO:
            log_debug("IPC_REQ_PRTS_GET_OPERATOR_INFO");
            break;
        case IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH:
            log_debug("IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH");
            break;
        case IPC_REQ_SETTINGS_GET:
            log_debug("IPC_REQ_SETTINGS_GET");
            break;
        case IPC_REQ_SETTINGS_SET:
            log_debug("IPC_REQ_SETTINGS_SET");
            break;
        case IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH:
            log_debug("IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH");
            break;
        case IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH:
            log_debug("IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH");
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION:
            log_debug("IPC_REQ_OVERLAY_SCHEDULE_TRANSITION");
            break;
        case IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO:
            log_debug("IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO");
            break;
        case IPC_REQ_APP_EXIT:
            log_debug("IPC_REQ_APP_EXIT");
            break;
        default:
            log_debug("IPC_REQ_UNKNOWN (%d)", type);
            break;
    }
}

void ipc_print_res_type(ipc_resp_type_t type){
    switch(type){
        case IPC_RESP_OK:
            log_debug("IPC_RESP_OK");
            break;
        case IPC_RESP_ERROR_MSG_TOO_LONG:
            log_debug("IPC_RESP_ERROR_MSG_TOO_LONG");
            break;
        case IPC_RESP_ERROR_NOMEM:
            log_debug("IPC_RESP_ERROR_NOMEM");
            break;
        case IPC_RESP_ERROR_INVALID_REQUEST:
            log_debug("IPC_RESP_ERROR_INVALID_REQUEST");
            break;
        case IPC_RESP_ERROR_STATE_CONFLICT:
            log_debug("IPC_RESP_ERROR_STATE_CONFLICT");
            break;
        case IPC_RESP_ERROR_UNKNOWN:
            log_debug("IPC_RESP_ERROR_UNKNOWN");
            break;
        default:
            log_debug("IPC_RESP_UNKNOWN (%d)", type);
            break;
    }
}
#endif