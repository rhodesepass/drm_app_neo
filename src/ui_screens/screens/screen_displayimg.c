#include "screen_displayimg.h"

#include <string.h>

#include "screen_common.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"

// 扩列图：图片本体由 overlay/video plane 显示，这里只放标题/序号/路径/无图提示。
static struct {
    lv_obj_t *size;
    lv_obj_t *no_pic;
    lv_obj_t *path;
} self;

lv_obj_t *screen_displayimg_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "扩列信息");

    self.size = lv_label_create(root);
    lv_obj_set_pos(self.size, S(157), S(16)); lv_obj_set_width(self.size, S(120));
    add_style_label_large(self.size);
    lv_label_set_text(self.size, "");

    self.no_pic = lv_label_create(root);
    lv_obj_set_pos(self.no_pic, S(48), S(119));
    add_style_label_large(self.no_pic);
    lv_label_set_text(self.no_pic, "Prts Warning:\n设备内没有扩列信息图...\n请将图片复制到\n/dispimg");

    self.path = lv_label_create(root);
    lv_obj_set_pos(self.path, S(10), S(615)); lv_obj_set_width(self.path, S(340));
    lv_label_set_long_mode(self.path, LV_LABEL_LONG_DOT);
    add_style_label_small(self.path);
    lv_label_set_text(self.path, "");

    screen_displayimg_tick();
    return root;
}

void screen_displayimg_tick(void)
{
    const char *v;
    v = ui_backend_dispimg_size(); if (strcmp(v, lv_label_get_text(self.size)) != 0) lv_label_set_text(self.size, v);
    v = ui_backend_dispimg_path(); if (strcmp(v, lv_label_get_text(self.path)) != 0) lv_label_set_text(self.path, v);

    bool warn = ui_backend_dispimg_has_warning();
    bool hidden = lv_obj_has_flag(self.no_pic, LV_OBJ_FLAG_HIDDEN);
    if (warn == hidden) { // 需切换可见性
        if (warn) lv_obj_clear_flag(self.no_pic, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(self.no_pic, LV_OBJ_FLAG_HIDDEN);
    }
}
