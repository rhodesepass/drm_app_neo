//
// sim 的 ui_plane 实现 —— 桌面没有多 DRM 平面/DEBE，按决策维持 LVGL 满屏淡入，
// 这里把图层滑动做成 no-op (仅打印，便于看导航策略在走)。
// 设备侧会用 layer_animation 提供真正的图层滑动。
//
#include "ui_screens/ui_plane.h"
#include "utils/log.h"

void ui_plane_move(int from_y, int to_y, int duration_us, int delay_us)
{
    log_debug("[sim ui_plane] move %d -> %d (dur=%dus delay=%dus) [no-op, fade only]",
              from_y, to_y, duration_us, delay_us);
}
