#include "font_registry.h"
#include "ui_metrics.h"

#include <stdio.h>
#include <string.h>

#include "utils/log.h"

// 字体文件目录 (含 lv_fs 盘符前缀)。设备侧默认放 rootfs；sim 通过编译期覆盖。
#ifndef FONT_REGISTRY_DIR
#define FONT_REGISTRY_DIR "A:/root/res/fonts"
#endif

// FreeType 字形缓存条目数 (弱端取较小值，跑起来盯 RAM 再调)
#ifndef FONT_REGISTRY_GLYPH_CACHE_CNT
#define FONT_REGISTRY_GLYPH_CACHE_CNT 256
#endif

// 每个角色对应的字体文件名与默认 style
typedef struct {
    const char *filename;
    lv_freetype_font_style_t style;
} font_face_desc_t;

static const font_face_desc_t s_faces[FONT_ROLE_COUNT] = {
    [FONT_BODY]    = { "SourceHanSansSC-Regular.otf",        LV_FREETYPE_FONT_STYLE_NORMAL },
    [FONT_TITLE]   = { "SourceHanSerifSC-Heavy.otf",         LV_FREETYPE_FONT_STYLE_NORMAL },
    [FONT_DISPLAY] = { "BebasNeue.otf",                      LV_FREETYPE_FONT_STYLE_NORMAL },
    [FONT_ICON]    = { "Font-Awesome-7-Free-Solid-900.otf",  LV_FREETYPE_FONT_STYLE_NORMAL },
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
    log_info("font_registry: initialized (dir=%s, glyph_cache=%d)",
             FONT_REGISTRY_DIR, FONT_REGISTRY_GLYPH_CACHE_CNT);
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
    snprintf(path, sizeof(path), "%s/%s", FONT_REGISTRY_DIR, s_faces[role].filename);

    lv_font_t *font = lv_freetype_font_create(
        path,
        LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
        (uint32_t)px,
        s_faces[role].style);

    if (!font) {
        log_error("font_registry: failed to load %s @ %dpx", path, px);
        return LV_FONT_DEFAULT;
    }

    s_reg.entries[s_reg.count].role = role;
    s_reg.entries[s_reg.count].px = px;
    s_reg.entries[s_reg.count].font = font;
    s_reg.count++;

    log_info("font_registry: loaded role=%d %s @ %dpx", (int)role, s_faces[role].filename, px);
    return font;
}
