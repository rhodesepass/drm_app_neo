#include "ui_preview.h"

#include <lvgl/lvgl.h>

#include "screens/screen_warning.h"
#include "screens/screen_confirm.h"
#include "screens/screen_usbselect.h"
#include "utils/log.h"

int g_ui_preview_interval_ms = 0;

#define PREVIEW_STEP_COUNT 4

// 点按回调:预览只看排版,proceed/cancel 都不做事 (下一次轮播会把屏切走)。
static void preview_noop(void) {}

// 假文案尽量压满各屏文本框边界,方便检查换行/溢出/削顶。
static void preview_step(lv_timer_t *t)
{
    int idx = (int)(intptr_t)lv_timer_get_user_data(t);

    switch (idx) {
    case 0:
        // warning: title 上限 64、desc 上限 160,用足够长的中文压边界。
        screen_warning_show(NULL,
            "警告文案压边测试标题",
            "这是一段用来测试告警屏描述区排版的较长文案,检查两行换行与底部是否被削掉。",
            0);
        break;
    case 1:
        // 内部二次确认 (关机/格式化):大号第二行。
        screen_confirm_show2(
            "=PRTS二次确认=",
            "这是一段较长的二次确认说明文案,用来检查确认屏标题区换行与按钮布局。",
            preview_noop, preview_noop);
        break;
    case 2:
        // FIDO / UIX 确认:小号第二行,文案对齐 usb_aio_handler fido_ui_ipc。
        screen_confirm_show_uix(
            "FIDO 身份验证",
            "站点: example.very-long-relying-party.example.com\n账户: user_name_压边测试账户",
            preview_noop, preview_noop);
        break;
    default:
        // USB 功能选择 (含 FIDO 密钥按钮)。0xF:四个功能全部显示。
        screen_usbselect_show(1, 0xF);
        break;
    }

    lv_timer_set_user_data(t, (void *)(intptr_t)((idx + 1) % PREVIEW_STEP_COUNT));
}

void ui_preview_start(void)
{
    if (g_ui_preview_interval_ms <= 0) return;

    log_warn("==> UI 弹窗预览模式: 每 %d ms 轮播 warning/confirm/fido-uix/usbselect",
             g_ui_preview_interval_ms);

    lv_timer_t *t = lv_timer_create(preview_step, g_ui_preview_interval_ms, (void *)0);
    lv_timer_ready(t); // 立刻弹第一个,不等第一个间隔
}
