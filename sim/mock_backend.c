//
// sim 后端桩 —— 实现 ui_backend.h 的精简接口，让手写屏脱离设备子系统在桌面跑。
//
#include "ui_screens/ui_backend.h"
#include "utils/log.h"

static int32_t s_brightness = 5;

const char *ui_backend_version(void)
{
    return "SIM-skeleton";
}

int32_t ui_backend_brightness_get(void)
{
    return s_brightness;
}

void ui_backend_brightness_set(int32_t value)
{
    s_brightness = value;
    log_info("[mock] brightness = %d", value);
}
