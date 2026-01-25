// 扩列图展示 专用
#pragma once

#include <stdint.h>
#include "config.h"
#include <stdbool.h>

typedef struct {
    char img_path[DISPLAYIMG_MAX_PATH_LENGTH];
    bool is_gif;
} ui_displayimg_file_t;

typedef struct {
    int count;
    int index;
    ui_displayimg_file_t files[DISPLAYIMG_MAX_COUNT];
} ui_displayimg_t;

typedef enum {
    UI_DISPLAYIMG_FILE_TYPE_JPG = 0,
    UI_DISPLAYIMG_FILE_TYPE_PNG = 1,
    UI_DISPLAYIMG_FILE_TYPE_BMP = 2,
    UI_DISPLAYIMG_FILE_TYPE_GIF = 3,
    UI_DISPLAYIMG_FILE_TYPE_INVALID = -1,
} ui_displayimg_file_type_t;

// 自己添加的方法

void ui_displayimg_init();

void ui_displayimg_key_event(uint32_t key);
void ui_displayimg_force_dispimg(const char *path);


// EEZ回调不需要添加。