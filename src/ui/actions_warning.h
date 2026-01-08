// 警告页面 专用
#pragma once

typedef enum {
    UI_WARNING_NONE = 0,
    UI_WARNING_LOW_BATTERY = 1, // 电池电量严重不足
    UI_WARNING_ASSET_ERROR = 2, // 部分干员加载失败
} warning_type_t;

// 自己添加的方法
void ui_warning(warning_type_t type);
// EEZ回调不需要添加。