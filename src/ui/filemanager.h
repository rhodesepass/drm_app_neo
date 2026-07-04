#pragma once
#include "apps/apps_types.h"

// 设备侧文件管理器：存 apps 句柄 (在 lvgl 起步时调一次)。
// 实际挂载/聚焦走 ui_hook_filemanager_mount / ui_hook_filemanager_enter (screen_filemanager 调)。
void filemanager_init(apps_t *apps);
