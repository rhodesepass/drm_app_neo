#pragma once
#include <stdbool.h>

// warning本来就是线程安全封装，这里不需要重复造轮子。
typedef enum {
    UI_IPC_HELPER_REQ_TYPE_SET_CURRENT_SCREEN = 0,
    UI_IPC_HELPER_REQ_TYPE_FORCE_DISPIMG = 1,
} ui_ipc_helper_req_type_t;

typedef struct {
    ui_ipc_helper_req_type_t type;
    int target_screen;
    char dispimg_path[128];

    bool on_heap;
} ui_ipc_helper_req_t;

void ui_ipc_helper_init();
void ui_ipc_helper_destroy();
void ui_ipc_helper_request(ui_ipc_helper_req_t *req);