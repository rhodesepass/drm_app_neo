#include "ui_theme.h"
#include "font_registry.h"
#include "ui_screens/styles.h"

// 配色方案：name 作设置屏下拉项，dark 决定 LVGL 基础主题(卡片/文字/滚动条)深浅，
// pal 是本方案的语义色表 (0xRRGGBB)。强调色 primary/warning/danger/success 一律选
// 中等饱和度以便白字可读；neutral/surface/bg 与 dark 标记的明暗保持一致。
typedef struct {
    const char *name;
    bool        dark;
    uint32_t    pal[UI_C_COUNT];
} ui_theme_preset_t;

static const ui_theme_preset_t s_presets[] = {
    { .name = "深色", .dark = true, .pal = {
        [UI_C_PRIMARY]=0x20679f, [UI_C_PRIMARY_FOCUS]=0x398ed0,
        [UI_C_WARNING]=0x8b7200,
        [UI_C_DANGER]=0xb93030,  [UI_C_DANGER_FOCUS]=0xa63737,
        [UI_C_SUCCESS]=0x149b5b,
        [UI_C_ACCENT]=0x67d9ec,
        [UI_C_NEUTRAL]=0x494947, [UI_C_MUTED]=0x919197, [UI_C_SURFACE]=0x3a3a3a,
        [UI_C_INFO]=0x2c3cbd,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0x1a1a1a,
        [UI_C_TEXT]=0xececec } },

    { .name = "浅色", .dark = false, .pal = {
        [UI_C_PRIMARY]=0x2f7dbf, [UI_C_PRIMARY_FOCUS]=0x4a9ad8,
        [UI_C_WARNING]=0xc79200,
        [UI_C_DANGER]=0xd23b3b,  [UI_C_DANGER_FOCUS]=0xe05555,
        [UI_C_SUCCESS]=0x1a9e63,
        [UI_C_ACCENT]=0x2f9fb5,
        [UI_C_NEUTRAL]=0xc8c8c6, [UI_C_MUTED]=0x9a9aa0, [UI_C_SURFACE]=0xf2f1f6,
        [UI_C_INFO]=0x3a4bd0,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0xeceef2,
        [UI_C_TEXT]=0x1e1e20 } },

    { .name = "Nord", .dark = true, .pal = {
        [UI_C_PRIMARY]=0x5e81ac, [UI_C_PRIMARY_FOCUS]=0x81a1c1,
        [UI_C_WARNING]=0xd08770,
        [UI_C_DANGER]=0xbf616a,  [UI_C_DANGER_FOCUS]=0xd0777f,
        [UI_C_SUCCESS]=0x8aa872,
        [UI_C_ACCENT]=0x88c0d0,
        [UI_C_NEUTRAL]=0x434c5e, [UI_C_MUTED]=0x4c566a, [UI_C_SURFACE]=0x3b4252,
        [UI_C_INFO]=0xb48ead,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0x2e3440,
        [UI_C_TEXT]=0xeceff4 } },

    { .name = "Gruvbox", .dark = true, .pal = {
        [UI_C_PRIMARY]=0x458588, [UI_C_PRIMARY_FOCUS]=0x83a598,
        [UI_C_WARNING]=0xd79921,
        [UI_C_DANGER]=0xcc241d,  [UI_C_DANGER_FOCUS]=0xfb4934,
        [UI_C_SUCCESS]=0x98971a,
        [UI_C_ACCENT]=0x8ec07c,
        [UI_C_NEUTRAL]=0x3c3836, [UI_C_MUTED]=0x665c54, [UI_C_SURFACE]=0x32302f,
        [UI_C_INFO]=0xb16286,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0x282828,
        [UI_C_TEXT]=0xebdbb2 } },

    { .name = "Solarized 深", .dark = true, .pal = {
        [UI_C_PRIMARY]=0x268bd2, [UI_C_PRIMARY_FOCUS]=0x3a9fe6,
        [UI_C_WARNING]=0xb58900,
        [UI_C_DANGER]=0xdc322f,  [UI_C_DANGER_FOCUS]=0xf0453f,
        [UI_C_SUCCESS]=0x859900,
        [UI_C_ACCENT]=0x2aa198,
        [UI_C_NEUTRAL]=0x073642, [UI_C_MUTED]=0x586e75, [UI_C_SURFACE]=0x073642,
        [UI_C_INFO]=0x6c71c4,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0x002b36,
        [UI_C_TEXT]=0x93a1a1 } },

    { .name = "Solarized 浅", .dark = false, .pal = {
        [UI_C_PRIMARY]=0x268bd2, [UI_C_PRIMARY_FOCUS]=0x1a6fb0,
        [UI_C_WARNING]=0xb58900,
        [UI_C_DANGER]=0xdc322f,  [UI_C_DANGER_FOCUS]=0xb0211f,
        [UI_C_SUCCESS]=0x859900,
        [UI_C_ACCENT]=0x2aa198,
        [UI_C_NEUTRAL]=0xeee8d5, [UI_C_MUTED]=0x93a1a1, [UI_C_SURFACE]=0xeee8d5,
        [UI_C_INFO]=0x6c71c4,    [UI_C_ON_ACCENT]=0xffffff, [UI_C_BG]=0xfdf6e3,
        [UI_C_TEXT]=0x073642 } },
};
#define UI_THEME_COUNT ((int)(sizeof(s_presets) / sizeof(s_presets[0])))

static int g_id = 0;

int         ui_theme_count(void)   { return UI_THEME_COUNT; }
const char *ui_theme_name(int id)  { return (id >= 0 && id < UI_THEME_COUNT) ? s_presets[id].name : ""; }
int         ui_theme_current(void) { return g_id; }
bool        ui_theme_is_dark(void) { return s_presets[g_id].dark; }

lv_color_t ui_color(ui_color_role_t r)
{
    if (r < 0 || r >= UI_C_COUNT) return lv_color_hex(0xff00ff);
    return lv_color_hex(s_presets[g_id].pal[r]);
}

void ui_theme_apply(int id)
{
    if (id < 0) id = 0;
    if (id >= UI_THEME_COUNT) id = UI_THEME_COUNT - 1;
    g_id = id;

    lv_display_t *disp = lv_display_get_default();
    lv_theme_t *th = lv_theme_default_init(
        disp,
        ui_color(UI_C_PRIMARY),
        ui_color(UI_C_ACCENT),
        s_presets[id].dark,
        font_get(FONT_BODY, 14));   // 中文字体作主题默认字体 -> dropdown 展开列表等默认控件不再豆腐块
    if (disp && th) lv_display_set_theme(disp, th);

    styles_apply_palette();          // 共享 style 按新表重着色
    lv_obj_report_style_change(NULL); // 刷新全场
}
