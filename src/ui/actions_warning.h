// 警告页面 专用
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_WARNING_NONE = 0,
    UI_WARNING_LOW_BATTERY = 1, // 电池电量严重不足
    UI_WARNING_ASSET_ERROR = 2, // 部分干员加载失败
    UI_WARNING_SD_MOUNT_ERROR = 3, // SD卡挂载失败
    UI_WARNING_PRTS_CONFLICT = 4, // PRTS冲突
    UI_WARNING_NO_ASSETS = 5, // 没有干员素材
    UI_WARNING_NOT_IMPLEMENTED = 6, // 未实现
    UI_WARNING_APP_NO_DIRECT_START = 7, // APP 不支持直接启动，需要通过关联的拓展名启动
    UI_WARNING_APP_LOAD_ERROR = 8, // 部分APP加载失败
} warning_type_t;

typedef struct {
    char *title;
    char *desc;
    char *icon;
    uint32_t color;
    bool str_on_heap;
} warning_info_t;

// 自己添加的方法
void ui_warning(warning_type_t type);
void ui_warning_custom(char* title, char* desc, char* icon, uint32_t color);
void ui_warning_init();
void ui_warning_destroy();
// EEZ回调不需要添加。