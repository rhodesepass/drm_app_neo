#include "screen_warning.h"

#include <string.h>

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui/ui_theme.h"
#include "ui_metrics.h"
#include "icons.h"

static struct {
    lv_obj_t *root, *icon, *title, *desc;
    char i[16], t[64], d[160];
    uint32_t color; // 0 = 用语义 danger 底 (取当前主题色)；非 0 = 调用方指定的固定色
} self;

// 告警屏是瞬态 (显示 3s 即消)，每次 show 直接按当前主题解析底色即可，无需随实时切主题翻转。
static void apply_bg(lv_obj_t *root)
{
    lv_color_t c = self.color ? lv_color_hex(self.color) : ui_color(UI_C_DANGER);
    lv_obj_set_style_bg_color(root, c, LV_PART_MAIN | LV_STATE_DEFAULT);
}

lv_obj_t *screen_warning_create(void)
{
    lv_obj_t *root = ui_screen_root();
    self.root = root;
    apply_bg(root);

    self.icon = lv_label_create(root);
    lv_obj_set_pos(self.icon, S(5), S(5));
    add_style_fa_label(self.icon);
    lv_label_set_text(self.icon, self.i[0] ? self.i : UI_ICON_TRIANGLE_EXCLAMATION);

    self.title = lv_label_create(root);
    lv_obj_set_pos(self.title, S(76), S(4)); lv_obj_set_width(self.title, S(275));
    add_style_label_large(self.title);
    lv_label_set_text(self.title, self.t[0] ? self.t : "警告");

    self.desc = lv_label_create(root);
    lv_obj_set_pos(self.desc, S(76), S(32)); lv_obj_set_size(self.desc, S(275), S(34));
    add_style_label_small(self.desc);
    lv_label_set_text(self.desc, self.d);

    return root;
}

void screen_warning_show(const char *icon, const char *title, const char *desc, uint32_t color)
{
    lv_strlcpy(self.i, icon ? icon : UI_ICON_TRIANGLE_EXCLAMATION, sizeof(self.i));
    lv_strlcpy(self.t, title ? title : "警告", sizeof(self.t));
    lv_strlcpy(self.d, desc ? desc : "", sizeof(self.d));
    self.color = color;
    if (self.icon) {
        lv_label_set_text(self.icon, self.i);
        lv_label_set_text(self.title, self.t);
        lv_label_set_text(self.desc, self.d);
        apply_bg(self.root);
    }
    screen_show(SCREEN_WARNING);
}
