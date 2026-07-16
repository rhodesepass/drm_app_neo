#pragma once

#include "lvgl.h"

#define KEY_ENC_EVDEV_MAX_FDS 16

typedef struct {
    lv_indev_t * indev;
    void (*input_cb)(uint32_t key);
    /* 非空则只打开该路径；空字符串则扫描 /dev/input/event* */
    char dev_path[128];
    int evdev_fds[KEY_ENC_EVDEV_MAX_FDS];
    int evdev_fd_count;
} key_enc_evdev_t;


lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev);

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev);
