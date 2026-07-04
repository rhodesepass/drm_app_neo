#include "ui_enums.h"
#include "config.h"
#include <stdint.h>
#include <pthread.h>
#pragma once

// 控制字
typedef struct{
    unsigned int lowbat_trip:1;
    unsigned int no_intro_block:1;
    unsigned int no_overlay_block:1;

}   settings_ctrl_word_t;

typedef struct {
    uint64_t magic; // EPASCONF
    uint32_t version;
    int brightness;
    sw_interval_t switch_interval;
    sw_mode_t switch_mode;
    usb_mode_t usb_mode;
    settings_ctrl_word_t ctrl_word;
    uint8_t theme_id;     // 配色方案索引 (见 ui_theme 预设表, 0=深色)
    pthread_mutex_t mtx;
} settings_t;

#define SETTINGS_LENGTH (sizeof(settings_t) - sizeof(pthread_mutex_t))

void settings_init(settings_t *settings);
void settings_update(settings_t *settings);
void settings_set_usb_mode(usb_mode_t usb_mode);
void settings_lock(settings_t *settings);
void settings_unlock(settings_t *settings);
void settings_destroy(settings_t *settings);