#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PRTS Timer / Scheduler
 *
 * 约束/语义（按用户确认）：
 * - 时间单位：对外接口全部使用 us（微秒）
 * - 精度：毫秒级即可（内部用 POSIX timer + CLOCK_MONOTONIC）
 * - 回调执行：允许并发执行；由用户保证回调执行时间不会超过 interval
 * - destroy/cancel：立即返回；保证之后不再触发即可（不等待正在运行的回调结束）
 * - one-shot / 有限次数：触发完成后自动取消并回收内部资源
 * - safe no-op：对已结束/已销毁的句柄，cancel/destroy 需要安全 no-op（返回 0）
 *
 * 单实例约束（S）：
 * - 进程内只允许 init 一个 prts_timer_t 实例（为了在 SIGEV_THREAD trampoline 中安全定位 tm）
 */

typedef uint64_t prts_timer_handle_t;
typedef void (*prts_timer_cb)(void *userdata,bool is_last);

#ifndef PRTS_TIMER_MAX
#define PRTS_TIMER_MAX 1024
#endif

typedef struct {
    uint32_t gen;          // generation（用于 handle 校验；同时低 16 bit 用于 SIGEV_THREAD 的 sival_int）
    bool active;
    timer_t t;

    prts_timer_cb cb;
    void *userdata;

    uint64_t interval_us;
    int64_t remaining;     // -1: infinite; >0: remaining fires
} prts_timer_slot_t;

typedef struct prts_timer {
    pthread_mutex_t mtx;
    bool mtx_inited;
    bool inited;

    prts_timer_slot_t slots[PRTS_TIMER_MAX + 1]; // id: 1..PRTS_TIMER_MAX
    uint16_t free_ids[PRTS_TIMER_MAX];
    uint16_t free_top; // 栈顶索引：当前可用数量（push: free_ids[free_top++]=id; pop: id=free_ids[--free_top]）
} prts_timer_t;

int prts_timer_init(prts_timer_t *tm);
int prts_timer_destroy(prts_timer_t *tm);

/**
 * 创建一个定时器
 *
 * @param out            返回句柄（handle = (gen<<32)|id）
 * @param start_delay_us 首次触发延迟(us)。若为 0，则默认使用 interval_us（若 interval_us==0 则内部使用 1us 以确保启动）
 * @param interval_us    周期(us)。无限/有限循环需要 >0；one-shot 可为 0（仅按 start_delay_us 触发一次）
 * @param fire_count     -1: 无限；>=1: 触发次数（1 为单次；>1 为有限次数）
 * @param cb             回调
 * @param userdata       回调参数
 */
int prts_timer_create(prts_timer_handle_t *out,
                      uint64_t start_delay_us,
                      uint64_t interval_us,
                      int64_t fire_count,
                      prts_timer_cb cb,
                      void *userdata);

int prts_timer_cancel(prts_timer_handle_t handle);

#ifdef __cplusplus
}
#endif