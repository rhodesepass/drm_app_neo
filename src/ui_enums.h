#pragma once
//
// 设置/屏标识枚举 —— 原在 EEZ 生成的 vars.h，被核心非 UI 代码 (settings/prts/ipc) 依赖。
// 拆 EEZ 后搬到这里独立保留。**数值必须与原 vars.h 一致**：curr_screen_t 经 IPC 跨进程
// 传给 apps 子进程，改值会错位。
//
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    sw_mode_t_SW_MODE_SEQUENCE = 0,
    sw_mode_t_SW_MODE_RANDOM = 1,
    sw_mode_t_SW_MODE_MANUAL = 2
} sw_mode_t;

typedef enum {
    sw_interval_t_SW_INTERVAL_1MIN = 0,
    sw_interval_t_SW_INTERVAL_3MIN = 1,
    sw_interval_t_SW_INTERVAL_5MIN = 2,
    sw_interval_t_SW_INTERVAL_10MIN = 3,
    sw_interval_t_SW_INTERVAL_30MIN = 4
} sw_interval_t;

typedef enum {
    usb_mode_t_MTP = 0,
    usb_mode_t_SERIAL = 1,
    usb_mode_t_RNDIS = 2,
    usb_mode_t_NONE = 3,
    usb_mode_t_EPMANAGER = 4
} usb_mode_t;

// 屏标识。IPC 用此枚举 (ipc_helper req->target_screen / ipc_handler 当前屏查询)。
typedef enum {
    curr_screen_t_SCREEN_MAINMENU = 0,
    curr_screen_t_SCREEN_OPLIST = 1,
    curr_screen_t_SCREEN_SYSINFO = 2,
    curr_screen_t_SCREEN_SPINNER = 3,
    curr_screen_t_SCREEN_DISPLAYIMG = 4,
    curr_screen_t_SCREEN_FILEMANAGER = 5,
    curr_screen_t_SCREEN_SETTINGS = 6,
    curr_screen_t_SCREEN_WARNING = 7,
    curr_screen_t_SCREEN_CONFIRM = 8,
    curr_screen_t_SCREEN_APPLIST = 9,
    curr_screen_t_SCREEN_USBSELECT = 10
} curr_screen_t;
