#include "screen_filemanager.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"

// 文件管理器：设备侧由 lv_file_explorer 填充 (create_filemanager)。
// sim 暂以占位说明 + 返回键代替 (浏览本机 fs 非本骨架重点)。
static void on_back(lv_event_t *e) { (void)e; screen_show(SCREEN_MAINMENU); }

lv_obj_t *screen_filemanager_create(void)
{
    lv_obj_t *root = ui_screen_root();
    ui_header(root, "文件");

    lv_obj_t *note = lv_label_create(root);
    lv_obj_set_pos(note, S(25), S(120)); lv_obj_set_width(note, S(310));
    add_style_label_small(note);
    lv_label_set_text(note, "文件管理器\n(设备上为 lv_file_explorer，浏览 NAND/SD)\nsim 占位。");

    ui_text_button(root, 23, 574, 316, 51, 0, "返回", on_back);
    return root;
}
