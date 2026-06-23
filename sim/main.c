//
// PC 模拟器入口 —— 在桌面用 SDL 跑手写 LVGL 屏 + FreeType 字体 registry。
//
// 分辨率档由 config.h 的屏选宏决定 (与设备同一开关)：
//   USE_360_640_SCREEN  -> 360x640  (UI_SCALE=1)
//   USE_720_1280_SCREEN -> 720x1280 (UI_SCALE=2)
//
#include <lvgl/lvgl.h>
#include <SDL2/SDL.h>
#include <stdlib.h>

#include "config.h"
#include "ui_metrics.h"
#include "ui_screens/screen_manager.h"
#include "ui/font_registry.h"
#include "utils/log.h"
#include "sim_sdl_window.h"

static uint32_t sim_tick_cb(void)
{
    return SDL_GetTicks();
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    log_info("==> EPass UI Simulator starting (%dx%d, UI_SCALE=%d)",
             UI_WIDTH, UI_HEIGHT, UI_SCALE);

    lv_init();
    lv_tick_set_cb(sim_tick_cb);

    lv_display_t *disp = sim_window_create(UI_WIDTH, UI_HEIGHT);
    if (!disp) {
        log_error("failed to create SDL window");
        return 1;
    }

    // 物理键回调走导航(切屏)，对应设备侧 key_enc_evdev.input_cb。
    sim_window_set_key_cb(screens_handle_key);

    // 字体须在建屏前就绪
    font_registry_init();
    screens_init();
    lv_indev_set_group(sim_window_indev(), screens_group());

    // 调试便利：SIM_SCREEN=<id> 直接跳到某屏 (见 screen_id_t 顺序)
    const char *scr = getenv("SIM_SCREEN");
    if (scr) {
        screen_show((screen_id_t)atoi(scr));
    }

    while (1) {
        uint32_t idle = lv_timer_handler();
        screens_tick();
        if (idle > 16) idle = 16;
        SDL_Delay(idle ? idle : 1);
    }
    return 0;
}
