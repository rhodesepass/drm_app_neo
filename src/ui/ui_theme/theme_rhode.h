#pragma once
//
// Rhode 主题（类表 + preset）—— 独立文件。
//
// 一套主题在 ui_theme/ 下自成一体：theme_rhode.c 里同时定义类表
// （按钮外观）和 preset（名称 / 深浅 / 色表 / 类表指针），内部按「页面」分组。
// 注册表在 themes.c，切主题引擎在 ui_theme.c。
//
#include "ui/ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

// Rhode 类表 (按页: confirm/usbselect/mainmenu/oplist 的按钮与角标)。实现见 theme_rhode.c。
extern const ui_class_def_t ui_theme_classes_rhode[];
// Rhode preset（名称 / 深浅 / 色表 / 类表 / 主菜单图标 / 坐标偏移）。实现见 theme_rhode.c。
extern const ui_theme_preset_t ui_theme_preset_rhode;

#ifdef __cplusplus
}
#endif
