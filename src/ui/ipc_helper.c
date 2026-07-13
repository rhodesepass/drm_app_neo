#include "ui/ipc_helper.h"
#include "config.h"
#include "utils/spsc_queue.h"
#include "lvgl.h"
#include <stdlib.h>
#include "ui_screens/ui_services.h"
#include "ui_screens/screen_manager.h"
#include "ui_screens/screens/screen_confirm.h"
#include "ui_screens/screens/screen_usbselect.h"
#include "ui/uix_session.h"

// 因为LVGL不是线程安全的。所有UI的写入，都需要在lvgl的线程内部自己完成。
// 所以需要一个helper来帮助我们完成这个工作。

static spsc_bq_t g_ui_ipc_queue;
static lv_timer_t *g_ui_ipc_timer = NULL;

// 当前正在展示的 UIX 会话（LVGL 线程私有）
static uint32_t g_uix_shown_id = 0;

static void uix_confirm_proceed(void) {
    uix_session_finish(g_uix_shown_id, UIX_CONFIRMED, 0);
    g_uix_shown_id = 0;
}
static void uix_confirm_cancel(void) {
    uix_session_finish(g_uix_shown_id, UIX_DENIED, 0);
    g_uix_shown_id = 0;
}

// 交互屏还在最前时收屏，否则不打扰用户当前所在界面
static void uix_dismiss_ui(void){
    screen_id_t cur = screens_current();
    if(cur == SCREEN_CONFIRM || cur == SCREEN_USBSELECT){
        screen_show(SCREEN_SPINNER);
    }
    g_uix_shown_id = 0;
}

static void uix_show(void){
    char title[UIX_TITLE_MAX];
    char desc[UIX_DESC_MAX];
    uint32_t id = 0, mask = 0;
    uix_kind_t kind = uix_session_snapshot(&id, title, sizeof(title), desc, sizeof(desc), &mask);
    switch(kind){
        case UIX_KIND_CONFIRM:
            g_uix_shown_id = id;
            screen_confirm_show_uix(title, desc, uix_confirm_proceed, uix_confirm_cancel);
            break;
        case UIX_KIND_USB_SELECT:
            g_uix_shown_id = id;
            screen_usbselect_show(id, mask);
            break;
        default:
            break; // 会话已被撤销，弹屏请求作废
    }
}

static void ui_ipc_helper_timer_cb(lv_timer_t *timer){
    (void)timer;
    ui_ipc_helper_req_t *req;
    if(spsc_bq_try_pop(&g_ui_ipc_queue, (void **)&req) == 0){
        switch(req->type){
            case UI_IPC_HELPER_REQ_TYPE_SET_CURRENT_SCREEN:
                ui_schedule_screen_transition((curr_screen_t)req->target_screen);
                break;
            case UI_IPC_HELPER_REQ_TYPE_FORCE_DISPIMG:
                ui_displayimg_force_dispimg(req->dispimg_path);
                break;
            case UI_IPC_HELPER_REQ_TYPE_REFRESH_OPLIST:
                // 干员素材重载后丢弃缓存屏，下次进入按新数据重建。
                screens_rebuild(SCREEN_OPLIST);
                // 同一次素材刷新 (usb_aio_handler 拔盘后触发) 也可能带来新的
                // 扩列图文件，联动重扫 /dispimg；扩列图屏自身按 tick diff 刷新，无需 rebuild。
                ui_displayimg_rescan();
                break;
            case UI_IPC_HELPER_REQ_TYPE_UIX_SHOW:
                uix_show();
                break;
            case UI_IPC_HELPER_REQ_TYPE_UIX_DISMISS:
                uix_dismiss_ui();
                break;
        }
        if(req->on_heap){
            free(req);
        }
    }
    // UIX 会话超时（用户一直不操作）：置 TIMEOUT 并收屏
    if(uix_session_tick()){
        uix_dismiss_ui();
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