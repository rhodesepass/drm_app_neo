#pragma once
//
// ui_theme 索引 —— 主题机制的统一入口。
//
// 其他模块（styles 等）只需 include 本文件，不必逐个 include ui_theme/ 下的主题文件。
// 新增主题：在 src/ui/ui_theme/ 下放 theme_xxx.c/h，在本文件加一行 include，
// 再到 themes.c 的注册表里加一行 &ui_theme_preset_xxx。
//
#include "ui/ui_theme.h"              // 核心：类型 / preset / API / 注册表声明
#include "ui/ui_theme/theme_default.h" // 默认类表 (advanced=false 主题的回退)
#include "ui/ui_theme/theme_old.h"     // Old（修改前 main 分支深色外观）
