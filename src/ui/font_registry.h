#pragma once
//
// font_registry —— 运行时 FreeType 字体的单一真值源 (UI 与 overlay 共用)
//
// 逻辑角色 (标题/正文/数字/图标) × 字号 -> 缓存的 lv_font_t*。
// 字号以 360 基准书写，font_get 内部套 S() 换算到当前目标。
// 句柄在 init / 进屏时建一次，按 (role, px) 缓存，严禁每帧重建。
//
// 字体走 FreeType stdio (LVGL port 关闭，无 lv_fs 盘符)。设备侧目录运行时按可执行文件
// 同级 res/fonts 解析 (见 respath)；PC 模拟器用 -DFONT_REGISTRY_DIR=... 覆盖为仓库 font/。
//
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FONT_BODY = 0,   // CJK + 拉丁正文 / 干员名 (SourceHanSansSC-Regular)
    FONT_TITLE,      // 重黑衬线标题 (SourceHanSerifSC-Heavy)
    FONT_DISPLAY,    // Bebas 数字 / 编号 / RHODES (BebasNeue)
    FONT_ICON,       // FontAwesome 7 图标 (Free-Solid-900)
    FONT_ROLE_COUNT
} font_role_t;

// 初始化 FreeType 与缓存。成功返回 0。须在创建任何屏 / 使用任何字体前调用一次。
int font_registry_init(void);

// 释放所有缓存字体与 FreeType。
void font_registry_deinit(void);

// 取 (role, base_px) 对应的字体；base_px 为 360 基准字号，内部套 S()。
// 同一 (role, px) 复用同一句柄。失败返回 LVGL 内建默认字体 (不为 NULL)。
const lv_font_t *font_get(font_role_t role, int base_px);

#ifdef __cplusplus
}
#endif
