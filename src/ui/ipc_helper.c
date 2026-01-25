#include "ui/ipc_helper.h"
#include "screens.h"
#include "utils/spsc_queue.h"
#include "lvgl.h"
#include <stdlib.h>
#include <ui/actions_displayimg.h>
#include <ui/scr_transition.h>

// 因为LVGL不是线程安全的。所有UI的写入，都需要在lvgl的线程内部自己完成。
// 所以需要一个helper来帮助我们完成这个工作。

static spsc_bq_t g_ui_ipc_queue;
static lv_timer_t *g_ui_ipc_timer = NULL;
extern objects_t objects;

static void ui_ipc_helper_timer_cb(lv_timer_t *timer){
    ui_ipc_helper_req_t *req;
    if(spsc_bq_try_pop(&g_ui_ipc_queue, (void **)&req) == 0){
        switch(req->type){
            case UI_IPC_HELPER_REQ_TYPE_SET_CURRENT_SCREEN:
                ui_schedule_screen_transition(req->target_screen);
                break;
            case UI_IPC_HELPER_REQ_TYPE_FORCE_DISPIMG:
                ui_displayimg_force_dispimg(req->dispimg_path);
                break;
        }
    }
    if(req->on_heap){
        free(req);
    }
}

void ui_ipc_helper_init(){
    spsc_bq_init(&g_ui_ipc_queue, 10);
    g_ui_ipc_timer = lv_timer_create(ui_ipc_helper_timer_cb, UI_IPC_HELPER_TIMER_TICK_PERIOD / 1000, NULL);
}

void ui_ipc_helper_destroy(){
    lv_timer_delete(g_ui_ipc_timer);
    spsc_bq_destroy(&g_ui_ipc_queue);
}

void ui_ipc_helper_request(ui_ipc_helper_req_t *req){
    spsc_bq_push(&g_ui_ipc_queue, (void *)req);
}