//
// key_enc_evdev 的 PC/SDL 后端 —— 同一头文件，链接期与 key_enc_evdev.c 二选一。
//
// 键源：drm_warpper_sdl 的事件泵把 SDL 键映射成 LVGL 键，按 KEYDOWN/KEYUP 分别经
// key_sdl_inject_press()/_release() 投进本文件的环形队列（跨线程，互斥锁保护）；
// LVGL 线程按 indev 轮询节拍 read_cb 弹出。队列空时保持上一次 press/release 状态，
// 因此按住键会持续回报 PRESSED（与设备 evdev 一致，长按/LV_EVENT_LONG_PRESSED 才成立）。
// press 时调 input_cb（screens_handle_key）驱动手写屏导航——恒在 LVGL 线程执行。
//
#include <pthread.h>
#include <string.h>

#include "driver/key_enc_evdev.h"
#include "utils/log.h"

#define KEYQ_MAX 32

typedef struct { uint32_t key; bool press; } keyev_t;
static keyev_t s_queue[KEYQ_MAX];
static int s_count;
static pthread_mutex_t s_mtx = PTHREAD_MUTEX_INITIALIZER;

static void keyq_push(uint32_t lv_key, bool press){
    pthread_mutex_lock(&s_mtx);
    if(s_count < KEYQ_MAX){
        s_queue[s_count].key = lv_key;
        s_queue[s_count].press = press;
        s_count++;
    }
    pthread_mutex_unlock(&s_mtx);
}

// drm_warpper_sdl 的事件泵线程调用
void key_sdl_inject_press(uint32_t lv_key){ keyq_push(lv_key, true); }
void key_sdl_inject_release(uint32_t lv_key){ keyq_push(lv_key, false); }

static void key_sdl_read_cb(lv_indev_t * indev, lv_indev_data_t * data){
    static uint32_t last_key = 0;
    static bool last_pressed = false;

    key_enc_evdev_t * ctx = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);

    keyev_t ev = { 0, false };
    bool have = false;
    pthread_mutex_lock(&s_mtx);
    if(s_count > 0){
        ev = s_queue[0];
        memmove(s_queue, s_queue + 1, (size_t)(--s_count) * sizeof(keyev_t));
        have = true;
    }
    pthread_mutex_unlock(&s_mtx);

    if(have){
        if(ev.press){
            data->key = ev.key;
            data->state = LV_INDEV_STATE_PRESSED;
            last_key = ev.key;
            last_pressed = true;
            if(ctx && ctx->input_cb){
                ctx->input_cb(ev.key); // 手写屏导航，恒在 LVGL 线程
            }
        } else {
            data->key = ev.key ? ev.key : last_key;
            data->state = LV_INDEV_STATE_RELEASED;
            last_pressed = false;
        }
        return;
    }

    // 队列空：维持上一次状态，按住键持续报 PRESSED
    data->key = last_key;
    data->state = last_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, key_sdl_read_cb);
    key_enc_evdev->indev = indev;
    key_enc_evdev->evdev_fd_count = 0; // PC 无 evdev，dev_path 忽略
    lv_indev_set_driver_data(indev, key_enc_evdev);
    log_info("==> [sdl] key backend initialized (arrows/enter/esc/end)");
    return indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev){
    lv_indev_delete(key_enc_evdev->indev);
}
