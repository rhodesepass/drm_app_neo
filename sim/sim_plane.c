//
// sim 的 ui_plane 实现 —— 用 lv_anim 缓动 viewport Y 偏移，由自含 SDL 后端
// (sim_sdl_window) 把整块 UI 纹理贴到带偏移的 dst 矩形，模拟设备 DEBE 图层滑动。
// 设备侧对应 layer_animation_ease_in_out_move(DRM_WARPPER_LAYER_UI, ...)。
//
#include "ui_screens/ui_plane.h"
#include "sim_sdl_window.h"
#include "lvgl/lvgl.h"

static void anim_exec_cb(void *var, int32_t v)
{
    (void)var;
    sim_window_set_plane_y(v);
    sim_window_present();  // 内容未变，LVGL 不会重 flush，手动推帧
}

void ui_plane_move(int from_y, int to_y, int duration_us, int delay_us)
{
    // spinner 两段式会连下两次 move：用唯一 var 避免 lv_anim 按 (var,exec) 去重
    // 把先注册的那段删掉。两段在时间上不重叠，可安全并存。
    static intptr_t token = 0;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, (void *)(++token));
    lv_anim_set_exec_cb(&a, anim_exec_cb);
    lv_anim_set_values(&a, from_y, to_y);
    lv_anim_set_duration(&a, (uint32_t)(duration_us / 1000));
    lv_anim_set_delay(&a, (uint32_t)(delay_us / 1000));
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}
