#pragma once

#include <stdint.h>

// PRTS timer 的 OS 后端：把 timer.c 里唯一贴着平台的三点（创建/销毁 OS 定时器、
// 触发时进哪个线程）挖出来。core(timer.c) 只跟这个接口打交道，slot/handle/gen
// 那套逻辑保持平台无关。posix = timer_create + SIGEV_THREAD；win32 = 定时器队列。
#ifdef _WIN32
// 实为 Win32 HANDLE（本质 void*）。这里不 include <windows.h>：timer.h 包含本头，
// 会被几乎所有 TU 拖进来，windows.h 的 uuid_t/RPC 类型会污染并撞工程自己的类型。
typedef void *os_timer_handle_t;
#else
#include <time.h>
typedef timer_t os_timer_handle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 创建并启动一个 OS 定时器。first_us 首次触发延迟；interval_us==0 表示单次
// (one-shot)，>0 为周期。触发时后端在独立线程调用 prts_timer_on_fire(packed_tag)。
// 返回 0，失败返回 -errno。
int os_timer_arm(os_timer_handle_t *h, uint64_t first_us, uint64_t interval_us, int packed_tag);

// 停止并销毁定时器；此后不再触发。
void os_timer_disarm(os_timer_handle_t h);

// 由后端在定时触发时调用（core 实现）。packed_tag = (gen<<16)|id。
void prts_timer_on_fire(int packed_tag);

#ifdef __cplusplus
}
#endif
