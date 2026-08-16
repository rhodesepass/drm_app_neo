#include "ui/ui_theme/theme.h"

// 所有主题 preset 的注册表。
// 新增主题：theme_xxx.c 里定义 ui_theme_preset_xxx，theme.h 里加 include，这里加一行。
const ui_theme_preset_t *const ui_theme_presets[] = {
    &ui_theme_preset_old,
};

const int ui_theme_preset_count = (int)(sizeof(ui_theme_presets) / sizeof(ui_theme_presets[0]));
