#include "apps/ipc_handler.h"
#include "apps/ipc_common.h"
#include "apps/apps_types.h"
#include <stdlib.h>
#include <ui/actions_warning.h>
#include <ui/ipc_helper.h>
#include <ui/scr_transition.h>
#include <utils/log.h>


// =========================================
// UI 子模块 处理方法
// =========================================
inline static int handle_ui_warning(ipc_req_t *req, ipc_resp_t *resp){
    ui_warning_custom(
        req->ui_warning.title, 
        req->ui_warning.desc, 
        req->ui_warning.icon, 
        req->ui_warning.color
    );
    return calculate_ipc_resp_size_by_req(req->type);
}

inline static int handle_ui_get_current_screen(ipc_req_t *req, ipc_resp_t *resp){
    resp->ui_current_screen.screen = ui_get_current_screen();
    resp->type = IPC_RESP_OK;
    return calculate_ipc_resp_size_by_req(req->type);
}

inline static int handle_ui_set_current_screen(ipc_req_t *req, ipc_resp_t *resp){
    ui_ipc_helper_req_t* helper_req = (ui_ipc_helper_req_t*)malloc(sizeof(ui_ipc_helper_req_t));
    if(helper_req == NULL){
        log_error("handle_ui_set_current_screen: malloc failed");
        resp->type = IPC_RESP_ERROR_NOMEM;
        return sizeof(ipc_resp_type_t);
    }
    helper_req->type = UI_IPC_HELPER_REQ_TYPE_SET_CURRENT_SCREEN;
    helper_req->target_screen = req->ui_set_current_screen.screen;
    helper_req->on_heap = true;
    ui_ipc_helper_request(helper_req);
    resp->type = IPC_RESP_OK;
    return calculate_ipc_resp_size_by_req(req->type);
}

inline static int handle_ui_force_dispimg(ipc_req_t *req, ipc_resp_t *resp){
    ui_ipc_helper_req_t* helper_req = (ui_ipc_helper_req_t*)malloc(sizeof(ui_ipc_helper_req_t));
    if(helper_req == NULL){
        log_error("handle_ui_force_dispimg: malloc failed");
        resp->type = IPC_RESP_ERROR_NOMEM;
        return sizeof(ipc_resp_type_t);
    }
    helper_req->type = UI_IPC_HELPER_REQ_TYPE_FORCE_DISPIMG;
    strncpy(helper_req->dispimg_path, req->ui_force_dispimg.path, sizeof(helper_req->dispimg_path));
    helper_req->on_heap = true;
    ui_ipc_helper_request(helper_req);
    resp->type = IPC_RESP_OK;
    return calculate_ipc_resp_size_by_req(req->type);
}


int apps_ipc_handler(apps_t *apps, uint8_t* rxbuf, size_t rxlen,uint8_t* txbuf, size_t txcap){
    ipc_req_t *req = (ipc_req_t *)rxbuf;
    ipc_resp_t *resp = (ipc_resp_t *)txbuf;
    size_t rx_expected_len = calculate_ipc_req_size(req->type);
    if (rxlen != rx_expected_len) {
        resp->type = IPC_RESP_ERROR_LENGTH_MISMATCH;
        return sizeof(ipc_resp_type_t);
    }

    switch(req->type){
        case IPC_REQ_UI_WARNING:
            return handle_ui_warning(req, resp);
        case IPC_REQ_UI_GET_CURRENT_SCREEN:
            return handle_ui_get_current_screen(req, resp);
        case IPC_REQ_UI_SET_CURRENT_SCREEN:
            return handle_ui_set_current_screen(req, resp);
        case IPC_REQ_UI_FORCE_DISPIMG:
            return handle_ui_force_dispimg(req, resp);
    }
    
    log_error("apps_ipc_handler: unknown request type: %d", req->type);
    resp->type = IPC_RESP_ERROR_UNKNOWN;
    return sizeof(ipc_resp_type_t);
}