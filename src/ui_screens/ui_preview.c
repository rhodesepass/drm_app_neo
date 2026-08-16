#include "ui_preview.h"

#include <lvgl/lvgl.h>
#include <stdlib.h>

#include "screen_manager.h"
#include "screens/screen_warning.h"
#include "screens/screen_confirm.h"
#include "screens/screen_usbselect.h"
#include "ui/ui_theme.h"
#include "utils/log.h"

int g_ui_preview_interval_ms = 0;
int g_ui_preview_theme_id    = -1;
int g_ui_preview_screen      = -1;

// 可预览的页面集合。弹窗 (warning/confirm/uix/usbselect) 用 _show 带假文案;
// 普通导航屏 (mainmenu/oplist/...) 用 screen_show 直接切过去看排版。
typedef enum {
    PREV_WARNING = 0,
    PREV_CONFIRM,
    PREV_UIX,
    PREV_USBSELECT,
    PREV_MAINMENU,
    PREV_OPLIST,
    PREV_SYSINFO,
    PREV_SETTINGS,
    PREV_APPLIST,
    PREV_COUNT
} preview_screen_t;

static const struct {
    const char      *name;   // 命令行页面名 (小写)
    preview_screen_t screen;
} kScreenNames[] = {
    { "warning",   PREV_WARNING   },
    { "confirm",   PREV_CONFIRM   },
    { "uix",       PREV_UIX       },
    { "fido",      PREV_UIX       },
    { "usbselect", PREV_USBSELECT },
    { "mainmenu",  PREV_MAINMENU  },
    { "oplist",    PREV_OPLIST    },
    { "sysinfo",   PREV_SYSINFO   },
    { "settings",  PREV_SETTINGS  },
    { "applist",   PREV_APPLIST   },
};

// 缺省轮播序列: 四个弹窗。
static const preview_screen_t kCycle[] = {
    PREV_WARNING, PREV_CONFIRM, PREV_UIX, PREV_USBSELECT,
};
#define CYCLE_COUNT ((int)(sizeof(kCycle) / sizeof(kCycle[0])))

// 大小写不敏感比较 (避开 strcasecmp/stricmp 的移植差异)。
static bool name_eq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

int ui_preview_theme_parse(const char *name)
{
    if (!name || !*name) return -1;
    // 数字下标
    if (name[0] >= '0' && name[0] <= '9') {
        int id = atoi(name);
        if (id >= 0 && id < ui_theme_count()) return id;
        return -1;
    }
    for (int i = 0; i < ui_theme_count(); i++) {
        if (name_eq(name, ui_theme_name(i))) return i;
    }
    return -1;
}

int ui_preview_screen_parse(const char *name)
{
    if (!name || !*name) return -1;
    for (size_t i = 0; i < sizeof(kScreenNames) / sizeof(kScreenNames[0]); i++) {
        if (name_eq(name, kScreenNames[i].name)) return (int)kScreenNames[i].screen;
    }
    return -1;
}

// 点按回调:预览只看排版,proceed/cancel 都不做事 (轮播时下一次会把屏切走)。
static void preview_noop(void) {}

// 假文案尽量压满各屏文本框边界,方便检查换行/溢出/削顶。
static void preview_show(preview_screen_t s)
{
    switch (s) {
    case PREV_WARNING:
        // warning: title 上限 64、desc 上限 160,用足够长的中文压边界。
        screen_warning_show(NULL,
            "警告文案压边测试标题",
            "这是一段用来测试告警屏描述区排版的较长文案,检查两行换行与底部是否被削掉。",
            0);
        break;
    case PREV_CONFIRM:
        // 内部二次确认 (关机/格式化):大号第二行。
        screen_confirm_show2(
            "=PRTS二次确认=",
            "这是一段较长的二次确认说明文案,用来检查确认屏标题区换行与按钮布局。",
            preview_noop, preview_noop);
        break;
    case PREV_UIX:
        // FIDO / UIX 确认:小号第二行,文案对齐 usb_aio_handler fido_ui_ipc。
        screen_confirm_show_uix(
            "FIDO 身份验证",
            "站点: example.very-long-relying-party.example.com\n账户: user_name_压边测试账户",
            preview_noop, preview_noop);
        break;
    case PREV_USBSELECT:
        // USB 功能选择 (含 FIDO 密钥按钮)。0xF:四个功能全部显示。
        screen_usbselect_show(1, 0xF);
        break;
    case PREV_MAINMENU:  screen_show(SCREEN_MAINMENU);  break;
    case PREV_OPLIST:    screen_show(SCREEN_OPLIST);    break;
    case PREV_SYSINFO:   screen_show(SCREEN_SYSINFO);   break;
    case PREV_SETTINGS:  screen_show(SCREEN_SETTINGS);  break;
    case PREV_APPLIST:   screen_show(SCREEN_APPLIST);   break;
    default: break;
    }
}

static void preview_step(lv_timer_t *t)
{
    int idx = (int)(intptr_t)lv_timer_get_user_data(t);
    preview_show(kCycle[idx % CYCLE_COUNT]);
    lv_timer_set_user_data(t, (void *)(intptr_t)((idx + 1) % CYCLE_COUNT));
}

// 单屏预览: 只显示指定页面一次 (经 LVGL 定时器, 与轮播同线程语义)。
static void preview_oneshot_cb(lv_timer_t *t)
{
    (void)t;
    preview_show((preview_screen_t)g_ui_preview_screen);
}

void ui_preview_start(void)
{
    if (g_ui_preview_interval_ms <= 0) return;

    if (g_ui_preview_screen >= 0) {
        log_warn("==> UI 单屏预览模式: screen=%d (theme=%d)", g_ui_preview_screen,
                 g_ui_preview_theme_id);
        lv_timer_t *t = lv_timer_create(preview_oneshot_cb, 1, NULL);
        lv_timer_set_repeat_count(t, 1); // 一次性,触发后自动删除
        lv_timer_ready(t);               // 立刻显示,不等间隔
        return;
    }

    log_warn("==> UI 弹窗预览模式: 每 %d ms 轮播 warning/confirm/fido-uix/usbselect",
             g_ui_preview_interval_ms);

    lv_timer_t *t = lv_timer_create(preview_step, g_ui_preview_interval_ms, (void *)0);
    lv_timer_ready(t); // 立刻弹第一个,不等第一个间隔
}
