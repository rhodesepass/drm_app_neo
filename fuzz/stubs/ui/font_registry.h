/* fuzz stub: 顶替真 font_registry.h，避免拖入 <lvgl/lvgl.h>。
   operators.c 的 epconfig 解析路径只用到 font_role_t 枚举，不调用任何 font_* 函数。
   枚举值必须与真头保持一致。 */
#pragma once

typedef enum {
    FONT_BODY = 0,
    FONT_TITLE,
    FONT_DISPLAY,
    FONT_ICON,
    FONT_ROLE_COUNT
} font_role_t;
