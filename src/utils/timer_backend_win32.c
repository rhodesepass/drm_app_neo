// PRTS timer 的 Windows 后端：定时器队列（CreateTimerQueueTimer）。回调在系统
// 线程池的独立线程执行、可并发，语义对齐 POSIX 的 SIGEV_THREAD。

#include "utils/timer_backend.h"

#include <windows.h>
#include <errno.h>
#include <stdint.h>

// us→ms 向上取整，避免亚毫秒被截成 0（core 保证 first_us>=1）。
static inline DWORD us_to_ms_ceil(uint64_t us)
{
    return (DWORD)((us + 999ULL) / 1000ULL);
}

static VOID CALLBACK win_timer_shim(PVOID param, BOOLEAN fired)
{
    (void)fired;
    prts_timer_on_fire((int)(intptr_t)param);
}

int os_timer_arm(os_timer_handle_t *h, uint64_t first_us, uint64_t interval_us, int packed_tag)
{
    DWORD due = us_to_ms_ceil(first_us);
    DWORD period = (interval_us != 0) ? us_to_ms_ceil(interval_us) : 0; // 0 ⇒ one-shot
    DWORD flags = (interval_us != 0) ? WT_EXECUTEDEFAULT : WT_EXECUTEONLYONCE;

    HANDLE t = NULL;
    if (!CreateTimerQueueTimer(&t, NULL, win_timer_shim,
                               (PVOID)(intptr_t)packed_tag, due, period, flags)) {
        return -EINVAL;
    }
    *h = t;
    return 0;
}

void os_timer_disarm(os_timer_handle_t h)
{
    // CompletionEvent=NULL：不等待正在运行的回调（立即返回语义）；回调跑完后
    // 系统自行回收。
    (void)DeleteTimerQueueTimer(NULL, h, NULL);
}
