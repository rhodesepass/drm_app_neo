#include "screen_filemanager.h"

#include "screen_common.h"
#include "screen_manager.h"
#include "styles.h"
#include "ui_metrics.h"

// 文件管理器：内容区由设备侧 ui_hook_filemanager_mount 填 lv_file_explorer；
// sim 弱默认不挂，仅显示占位说明。无返回按钮，ESC 直接回主菜单。

static void on_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    lv_group_t *g = screens_group();
    if (!g) return;
    lv_group_remove_all_objs(g);
    lv_group_set_wrap(g, false);
    ui_hook_filemanager_enter(g); // 设备: 文件表加入并聚焦
}

lv_obj_t *screen_filemanager_create(void)
{
    lv_obj_t *root = ui_screen_root_bare();
    ui_header(root, "文件");

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_set_pos(content, 0, S(40));
    lv_obj_set_size(content, S(360), S(600));
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *note = lv_label_create(content);
    lv_obj_set_pos(note, S(25), S(80)); lv_obj_set_width(note, S(310));
    add_style_label_small(note);
    lv_label_set_text(note, "文件管理器\n(设备上为 lv_file_explorer，浏览 NAND/SD)\nsim 占位。");

    ui_hook_filemanager_mount(content); // 设备: 清空 content 并建 explorer

    lv_obj_add_event_cb(root, on_load_start, LV_EVENT_SCREEN_LOAD_START, NULL);
    return root;
}
