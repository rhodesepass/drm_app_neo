// key as encoder with evdev support
#pragma once

#include "lvgl.h"


typedef struct {
    lv_indev_t * indev;
    void (*input_cb)(uint32_t key);
    char dev_path[128];
    int evdev_fd;
} key_enc_evdev_t;


lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev);

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev);