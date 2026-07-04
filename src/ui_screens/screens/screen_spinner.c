#include "screen_spinner.h"
#include "screen_common.h"
#include "styles.h"
#include "ui_metrics.h"

// spinner = "提交反馈" 过场屏 (停靠在屏幕下方隐藏，仅 intro 过渡时短暂可见)。
lv_obj_t *screen_spinner_create(void)
{
    lv_obj_t *root = ui_screen_root();

    lv_obj_t *sp = lv_spinner_create(root);
    lv_obj_set_pos(sp, S(20), S(5));
    lv_obj_set_size(sp, S(44), S(44));
    lv_obj_set_style_arc_width(sp, S(6), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(sp, S(6), LV_PART_MAIN | LV_STATE_DEFAULT);
    add_style_spinner_arc(sp);

    lv_obj_t *status = lv_label_create(root);
    lv_obj_set_pos(status, S(80), S(24));
    add_style_label_small(status);
    lv_label_set_text(status, "正在提交反馈至神经...");

    lv_obj_t *log = lv_label_create(root);
    lv_obj_set_pos(log, S(230), S(0));
    add_style_label_small(log);
    add_style_log_text(log);
    lv_label_set_text(log,
        "RDEP Connection\n==> Rhodes Island\n[OK] TLS Handshake\n[OK] ::43232->::22");

    return root;
}
