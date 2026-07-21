#pragma once

#include <stdbool.h>

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

/* 动画期间静音按键:置位后读回调抽干并丢弃所有排队事件 */
void key_enc_evdev_mute(bool muted);
