//
// ui_plane 设备实现 —— UI 平面滑动 = sunxi DEBE 图层 Y 坐标移动 (原 scr_transition.c)。
//
#include "ui_screens/ui_plane.h"
#include "render/layer_animation.h"
#include "config.h"   // DRM_WARPPER_LAYER_UI

static layer_animation_t *s_la;

void ui_plane_device_bind(void *layer_animation)
{
    s_la = (layer_animation_t *)layer_animation;
}

void ui_plane_move(int from_y, int to_y, int duration_us, int delay_us)
{
    if (!s_la) return;
    layer_animation_ease_in_out_move(s_la, DRM_WARPPER_LAYER_UI,
                                     0, from_y, 0, to_y, duration_us, delay_us);
}
