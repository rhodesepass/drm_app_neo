// PRTS timer 的 POSIX 后端：timer_create + SIGEV_THREAD + CLOCK_MONOTONIC。
// 逻辑与原 timer.c 内联实现逐字一致，仅把 slot 管理留在 core。

#include "utils/timer_backend.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

static inline void us_to_timespec(uint64_t us, struct timespec *out)
{
    out->tv_sec = (time_t)(us / 1000000ULL);
    out->tv_nsec = (long)((us % 1000000ULL) * 1000ULL);
}

// SIGEV_THREAD trampoline：只解出 packed_tag 交给 core，slot 校验/递减全在 core。
static void posix_timer_shim(union sigval sv)
{
    prts_timer_on_fire(sv.sival_int);
}

int os_timer_arm(os_timer_handle_t *h, uint64_t first_us, uint64_t interval_us, int packed_tag)
{
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = posix_timer_shim;
    sev.sigev_value.sival_int = packed_tag;

    timer_t t;
    if (timer_create(CLOCK_MONOTONIC, &sev, &t) != 0) {
        return -errno;
    }

    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    us_to_timespec(first_us, &its.it_value);
    us_to_timespec(interval_us, &its.it_interval); // interval_us==0 ⇒ one-shot

    if (timer_settime(t, 0, &its, NULL) != 0) {
        int err = errno;
        (void)timer_delete(t);
        return -err;
    }

    *h = t;
    return 0;
}

void os_timer_disarm(os_timer_handle_t h)
{
    (void)timer_delete(h);
}
