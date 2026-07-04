#include "screen_displayimg.h"

#include <string.h>

#include "screen_common.h"
#include "styles.h"
#include "ui_backend.h"
#include "ui_metrics.h"

// 扩列图：图片本体由 LVGL 在 UI 平面渲染 (lv_image / lv_gif)，外加标题/序号/路径/无图提示。
static struct {
    lv_obj_t *size;
    lv_obj_t *no_pic;
    lv_obj_t *path;
    lv_obj_t *img_box;    // 图片容器 (清空后重建 image/gif)
    char      shown_path[256];
} self;

lv_obj_t *screen_displayimg_create(void)
{
    memset(&self, 0, sizeof(self));
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "扩列信息");

    // 图片容器铺满，置底 (标题/路径浮在其上)。
    self.img_box = lv_obj_create(root);
    lv_obj_set_pos(self.img_box, 0, 0);
    lv_obj_set_size(self.img_box, S(UI_BASE_WIDTH), S(UI_BASE_HEIGHT));
    lv_obj_set_style_pad_all(self.img_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(self.img_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(self.img_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(self.img_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(self.img_box);

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

    // 图片本体：路径变化时清空重建 (gif/静图)。无图则只清空。
    const char *p = warn ? "" : ui_backend_dispimg_path();
    if (strcmp(p, self.shown_path) != 0) {
        lv_strlcpy(self.shown_path, p, sizeof(self.shown_path));
        lv_obj_clean(self.img_box);
        if (!warn && p[0]) {
            if (ui_backend_dispimg_is_gif()) {
                lv_obj_t *im = lv_gif_create(self.img_box);
                lv_gif_set_color_format(im, LV_COLOR_FORMAT_RGB565);
                lv_gif_set_src(im, p);
                lv_obj_center(im);
            } else {
                lv_obj_t *im = lv_image_create(self.img_box);
                lv_image_set_src(im, p);
                lv_obj_center(im);
            }
        }
    }
}
