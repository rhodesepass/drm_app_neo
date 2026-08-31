#pragma once
//
// Lonetrail（孤星）主题（类表 + preset）—— 独立文件。
//
// 设计：浅色奶油底 + 橙色强调，宫格按钮直角 + 装饰块，图标为孤星版码点；
// 类表颜色填角色 (UI_C_*)，切主题时由 palette 提供实际 hex。
// 内部按「页面」分组 (确认框/usbselect/主菜单/oplist)。
//
#include "ui/ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lonetrail 类表 (按页: confirm/usbselect/mainmenu/oplist 的按钮与角标)。实现见 theme_lonetrail.c。
extern const ui_class_def_t ui_theme_classes_lonetrail[];
// Lonetrail preset（名称 / 深浅 / 色表 / 类表 / 主菜单图标 / 页头钩子 / 坐标偏移）。实现见 theme_lonetrail.c。
extern const ui_theme_preset_t ui_theme_preset_lonetrail;

#ifdef __cplusplus
}
#endif
