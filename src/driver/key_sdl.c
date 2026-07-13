//
// key_enc_evdev 的 PC/SDL 后端 —— 同一头文件，链接期与 key_enc_evdev.c 二选一。
//
// 键源：drm_warpper_sdl 的事件泵把 SDL 键映射成 LVGL 键后经 key_sdl_inject()
// 投进本文件的环形队列（跨线程，互斥锁保护）；LVGL 线程按 indev 轮询节拍
// read_cb 弹出，一次按下拆成 PRESSED + RELEASED 两次回报（同设备 evdev 语义），
// 并在按下时调 input_cb（screens_handle_key）驱动手写屏导航——input_cb 因此
// 恒在 LVGL 线程执行，与设备侧一致。
//
#include <pthread.h>
#include <string.h>

#include "driver/key_enc_evdev.h"
#include "utils/log.h"

#define KEYQ_MAX 32

static uint32_t s_queue[KEYQ_MAX];
static int s_count;
static pthread_mutex_t s_mtx = PTHREAD_MUTEX_INITIALIZER;

// drm_warpper_sdl 的事件泵线程调用
void key_sdl_inject(uint32_t lv_key){
    pthread_mutex_lock(&s_mtx);
    if(s_count < KEYQ_MAX){
        s_queue[s_count++] = lv_key;
    }
    pthread_mutex_unlock(&s_mtx);
}

static void key_sdl_read_cb(lv_indev_t * indev, lv_indev_data_t * data){
    static uint32_t last_key = 0;
    static bool pending_release = false;

    key_enc_evdev_t * ctx = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);

    if(pending_release){
        pending_release = false;
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint32_t key = 0;
    pthread_mutex_lock(&s_mtx);
    if(s_count > 0){
        key = s_queue[0];
        memmove(s_queue, s_queue + 1, (size_t)(--s_count) * sizeof(uint32_t));
    }
    pthread_mutex_unlock(&s_mtx);

    if(key){
        data->key = key;
        data->state = LV_INDEV_STATE_PRESSED;
        last_key = key;
        pending_release = true;
        if(ctx && ctx->input_cb){
            ctx->input_cb(key); // 手写屏导航，恒在 LVGL 线程
        }
        return;
    }

    data->key = last_key;
    data->state = LV_INDEV_STATE_RELEASED;
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, key_sdl_read_cb);
    key_enc_evdev->indev = indev;
    key_enc_evdev->evdev_fd = -1; // PC 无 evdev，dev_path 忽略
    lv_indev_set_driver_data(indev, key_enc_evdev);
    log_info("==> [sdl] key backend initialized (arrows/enter/esc/end)");
    return indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev){
    lv_indev_delete(key_enc_evdev->indev);
}
