#pragma once
//
// Old 主题（类表 + preset）—— 复刻修改前 (main 分支) 的默认「深色」外观。
//
// 老观感：宫格 PRIMARY 底、聚焦变浅一档；确认按钮红/灰纯色底；oplist 条目
// 整块换色聚焦（灰底→青底、排序整块橙），没有左侧边框高亮条。颜色仍走角色，
// 由本文件色表提供实际 hex（与 main 分支 `ui_theme.c` 里的「深色」一致）。
// 内部按「页面」分组；坐标/文字走屏内基准 (旧版坐标)，ofs=NULL 无需偏移。
//
#include "ui/ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

// Old 类表 (按页: confirm/usbselect/mainmenu/oplist 的按钮与角标)。实现见 theme_old.c。
extern const ui_class_def_t ui_theme_classes_old[];
// Old preset（名称 / 深浅 / 色表 / 类表 / 主菜单图标）。实现见 theme_old.c。
extern const ui_theme_preset_t ui_theme_preset_old;

#ifdef __cplusplus
}
#endif
