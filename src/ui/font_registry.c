#include "font_registry.h"
#include "ui_metrics.h"

#include <stdio.h>
#include <string.h>

#include "utils/log.h"

// 字体文件目录。FreeType 关了 LVGL port (PORT=0) ⇒ 直接走 stdio 文件路径，无 lv_fs 盘符。
// FONT_REGISTRY_DIR 编译期给定字体目录:
//   - 设备侧: buildroot 顶层 CMake 从 pkg-config(epass-fonts) 取 fontsdir, 定为
//     /usr/share/fonts/epass (系统共享字体, 见 buildroot package/epass-fonts)。
//   - 模拟器: sim/CMakeLists 指向仓库 font/ 或兄弟仓 epass-fonts/original。
// 未定义此宏 (纯本地 dev 构建, 无 epass-fonts) 时, 回退到可执行文件同级 res/fonts (respath)。
#ifndef FONT_REGISTRY_DIR
#include "config.h"
#include "utils/respath.h"
#endif

// FreeType 字形缓存条目数 (弱端取较小值，跑起来盯 RAM 再调)
#ifndef FONT_REGISTRY_GLYPH_CACHE_CNT
#define FONT_REGISTRY_GLYPH_CACHE_CNT 256
#endif

// 每个角色对应的字体文件名与默认 style。
// lh_pct: 行高占字号的百分比, 用来覆写 FreeType 默认行高 (中文字体 hhea/typo metrics
//   常带较大 line gap, 单行文字上下留白多)。0 = 保持字体默认 (多行正文要靠它撑行距)。
typedef struct {
    const char *filename;
    lv_freetype_font_style_t style; 
    int lh_pct;
} font_face_desc_t;

static const font_face_desc_t s_faces[FONT_ROLE_COUNT] = {
    [FONT_BODY]    = { "SourceHanSansSC-Regular.otf",        LV_FREETYPE_FONT_STYLE_NORMAL, 110   },
    [FONT_TITLE]   = { "SourceHanSerifSC-Heavy.otf",         LV_FREETYPE_FONT_STYLE_NORMAL, 115 },
    [FONT_DISPLAY] = { "BebasNeue.otf",                      LV_FREETYPE_FONT_STYLE_NORMAL, 115 },
    [FONT_ICON]    = { "Font-Awesome-7-Free-Solid-900.otf",  LV_FREETYPE_FONT_STYLE_NORMAL, 0   },
};

// (role, px) -> lv_font_t* 缓存。组合很少，线性查找即可。
#define FONT_CACHE_MAX 32
typedef struct {
    font_role_t role;
    int px;            // 实际像素 (已套 S())
    lv_font_t *font;
} font_cache_entry_t;

static struct {
    bool inited;
    font_cache_entry_t entries[FONT_CACHE_MAX];
    int count;
} s_reg;

int font_registry_init(void)
{
    if (s_reg.inited) {
        return 0;
    }
    // FreeType 可能已被 LVGL 其它模块初始化过 (返回非 OK 即 "already initialized")，
    // 这种情况视为成功，继续按角色建字体即可。
    if (lv_freetype_init(FONT_REGISTRY_GLYPH_CACHE_CNT) != LV_RESULT_OK) {
        log_warn("font_registry: lv_freetype_init returned non-OK (likely already initialized), continuing");
    }
    s_reg.count = 0;
    s_reg.inited = true;
#ifdef FONT_REGISTRY_DIR
    log_info("font_registry: initialized (dir=%s, glyph_cache=%d)",
             FONT_REGISTRY_DIR, FONT_REGISTRY_GLYPH_CACHE_CNT);
#else
    log_info("font_registry: initialized (dir=%s/%s, glyph_cache=%d)",
             respath_dir(), RES_FONTS_SUBDIR, FONT_REGISTRY_GLYPH_CACHE_CNT);
#endif
    return 0;
}

void font_registry_deinit(void)
{
    if (!s_reg.inited) {
        return;
    }
    for (int i = 0; i < s_reg.count; i++) {
        if (s_reg.entries[i].font) {
            lv_freetype_font_delete(s_reg.entries[i].font);
            s_reg.entries[i].font = NULL;
        }
    }
    s_reg.count = 0;
    lv_freetype_uninit();
    s_reg.inited = false;
}

const lv_font_t *font_get(font_role_t role, int base_px)
{
    if (role < 0 || role >= FONT_ROLE_COUNT) {
        log_error("font_registry: invalid role %d", (int)role);
        return LV_FONT_DEFAULT;
    }
    if (!s_reg.inited && font_registry_init() != 0) {
        return LV_FONT_DEFAULT;
    }

    int px = S(base_px);

    // 命中缓存
    for (int i = 0; i < s_reg.count; i++) {
        if (s_reg.entries[i].role == role && s_reg.entries[i].px == px) {
            return s_reg.entries[i].font;
        }
    }

    if (s_reg.count >= FONT_CACHE_MAX) {
        log_error("font_registry: cache full (%d), reusing default", FONT_CACHE_MAX);
        return LV_FONT_DEFAULT;
    }

    char path[256];
#ifdef FONT_REGISTRY_DIR
    snprintf(path, sizeof(path), "%s/%s", FONT_REGISTRY_DIR, s_faces[role].filename);
#else
    snprintf(path, sizeof(path), "%s/%s/%s", respath_dir(), RES_FONTS_SUBDIR, s_faces[role].filename);
#endif

    lv_font_t *font = lv_freetype_font_create(
        path,
        LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
        (uint32_t)px,
        s_faces[role].style);

    if (!font) {
        log_error("font_registry: failed to load %s @ %dpx", path, px);
        return LV_FONT_DEFAULT;
    }

    // 收紧单行字体行高 (减少上下留白, 便于垂直居中)。多行角色 lh_pct=0 保持默认。
    if (s_faces[role].lh_pct > 0) {
        font->line_height = (px * s_faces[role].lh_pct) / 100;
    }

    // 文字角色挂 fallback 到同字号 ICON 字体: 中文字体无 FontAwesome 字形, 否则
    // dropdown 箭头(LV_SYMBOL_DOWN=U+F078)等混排符号会渲染成豆腐块。ICON 不设 fallback 避免递归。
    if (role != FONT_ICON) {
        font->fallback = font_get(FONT_ICON, base_px);
    }

    s_reg.entries[s_reg.count].role = role;
    s_reg.entries[s_reg.count].px = px;
    s_reg.entries[s_reg.count].font = font;
    s_reg.count++;

    log_info("font_registry: loaded role=%d %s @ %dpx", (int)role, s_faces[role].filename, px);
    return font;
}
