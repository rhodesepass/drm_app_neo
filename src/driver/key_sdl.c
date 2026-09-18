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
#include "driver/input_gestures.h"
#include "ui_screens/screen_manager.h"
#include "ui_screens/screens/screen_confirm.h"
#include "utils/log.h"

#define KEYQ_MAX 32

typedef struct { uint32_t key; bool press; uint32_t tick; uint64_t sequence; } keyev_t;
static keyev_t s_queue[KEYQ_MAX];
static int s_count;
static pthread_mutex_t s_mtx = PTHREAD_MUTEX_INITIALIZER;
static bool s_muted;
static bool s_reset;
static bool s_physical[256], s_down[256], s_blocked[256];
static uint64_t s_sequence, s_shutdown_boundary;
static uint32_t s_last_key;
static bool s_last_pressed, s_shutdown;
static power_gesture_t s_power;

static void keyq_push(uint32_t lv_key, bool press){
    if(lv_key >= 256) return;
    pthread_mutex_lock(&s_mtx);
    s_physical[lv_key] = press;
    if(s_muted){
        s_blocked[lv_key] = press;
        pthread_mutex_unlock(&s_mtx);
        return;
    }
    if(s_count < KEYQ_MAX){
        s_queue[s_count].key = lv_key;
        s_queue[s_count].press = press;
        s_queue[s_count].tick = lv_tick_get();
        s_queue[s_count].sequence = ++s_sequence;
        s_count++;
    } else {
        s_count = 0;
        memcpy(s_blocked, s_physical, sizeof(s_blocked));
        s_reset = true;
    }
    pthread_mutex_unlock(&s_mtx);
}

// drm_warpper_sdl 的事件泵线程调用
void key_sdl_inject_press(uint32_t lv_key){ keyq_push(lv_key, true); }
void key_sdl_inject_release(uint32_t lv_key){ keyq_push(lv_key, false); }

// 动画期间静音:清空队列并拦截读回调,丢弃这段时间攒下的键(与设备侧语义一致)
void key_enc_evdev_mute(bool muted){
    pthread_mutex_lock(&s_mtx);
    s_muted = muted;
    if(muted){
        s_count = 0;
        memcpy(s_blocked, s_physical, sizeof(s_blocked));
        memset(s_down, 0, sizeof(s_down));
        s_last_key = 0;
        s_last_pressed = false;
        memset(&s_power, 0, sizeof(s_power));
    }
    pthread_mutex_unlock(&s_mtx);
}

static void sync_shutdown(void){
    bool shutdown = screen_confirm_is_shutdown();
    if(shutdown != s_shutdown){
        pthread_mutex_lock(&s_mtx);
        s_shutdown_boundary = s_sequence;
        for(unsigned i = 0; i < 256; ++i)
            s_blocked[i] = s_blocked[i] || s_physical[i] || s_down[i];
        s_last_pressed = false;
        memset(&s_power, 0, sizeof(s_power));
        pthread_mutex_unlock(&s_mtx);
        s_shutdown = shutdown;
    }
}

static void dispatch_power(input_action_t action){
    switch(action){
    case INPUT_POWER_MENU:
        log_debug("[sdl] power: menu");
        screens_power_short();
        break;
    case INPUT_POWER_DISPLAYIMG:
        log_debug("[sdl] power: displayimg");
        screens_power_double();
        break;
    case INPUT_POWER_SHUTDOWN:
        log_debug("[sdl] power: shutdown");
        screens_power_long();
        break;
    default: break;
    }
}

static void key_sdl_read_cb(lv_indev_t * indev, lv_indev_data_t * data){
    key_enc_evdev_t * ctx = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);
    data->key = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    sync_shutdown();
    keyev_t ev = {0};
    bool have = false;
    bool power_is_down;
    bool queue_empty;
    pthread_mutex_lock(&s_mtx);
    if(s_reset){
        memset(s_down, 0, sizeof(s_down));
        s_last_pressed = false;
        memset(&s_power, 0, sizeof(s_power));
        s_reset = false;
    }
    if(s_muted){
        pthread_mutex_unlock(&s_mtx);
        return;
    }
    if(s_count > 0){
        ev = s_queue[0];
        memmove(s_queue, s_queue + 1, (size_t)(--s_count) * sizeof(keyev_t));
        have = true;
    }
    if(have){
        bool was_down = s_down[ev.key];
        s_down[ev.key] = ev.press;
        bool blocked = s_blocked[ev.key];
        if(!ev.press) s_blocked[ev.key] = false;
        if(blocked || (ev.press && was_down) ||
           (s_shutdown && ev.sequence <= s_shutdown_boundary)) have = false;
    }
    power_is_down = s_physical[LV_KEY_END];
    queue_empty = s_count == 0;
    pthread_mutex_unlock(&s_mtx);

    if(have && s_shutdown){
        if(ev.press && ev.key == LV_KEY_END) screen_confirm_shutdown_cancel();
        else if(ev.press && ev.key == LV_KEY_ENTER) screen_confirm_shutdown_proceed();
        sync_shutdown();
        return;
    }
    if(have && ev.key == LV_KEY_END){
        dispatch_power(power_gesture_key(&s_power, ev.press, ev.tick, false, false));
        have = false;
    }
    if(queue_empty && (!s_power.down || power_is_down))
        dispatch_power(power_gesture_tick(&s_power, lv_tick_get()));
    sync_shutdown();
    if(s_shutdown) return;
    if(have){
        if(ev.press){
            s_last_key = ev.key;
            s_last_pressed = true;
            if(ctx && ctx->input_cb){
                ctx->input_cb(ev.key);
            }
            sync_shutdown();
        } else {
            if(ev.key == s_last_key) s_last_pressed = false;
        }
    }
    if(s_shutdown || s_muted) return;
    data->key = s_last_key;
    data->state = s_last_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    screen_confirm_set_input_sources(false, true);
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
