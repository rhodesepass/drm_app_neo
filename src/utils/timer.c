// PRTS timer implementation

#include "utils/timer.h"

#include <errno.h>
#include <string.h>

#include "utils/log.h"
#include "utils/timer_backend.h"

// 单实例入口（S）：SIGEV_THREAD trampoline 通过它定位到 tm
static prts_timer_t *g_prts_tm_singleton = NULL;

static inline uint32_t handle_id(prts_timer_handle_t h) { return (uint32_t)(h & 0xFFFFFFFFu); }
static inline uint32_t handle_gen(prts_timer_handle_t h) { return (uint32_t)((h >> 32) & 0xFFFFFFFFu); }

// SIGEV_THREAD 只能可靠携带一个 (按值) union sigval，这里用 16bit id + 16bit gen 打包
static inline int pack_sigval_u32(uint16_t id, uint16_t gen)
{
    return (int)(((uint32_t)gen << 16) | (uint32_t)id);
}

static inline void unpack_sigval_u32(int packed, uint16_t *out_id, uint16_t *out_gen)
{
    uint32_t u = (uint32_t)packed;
    *out_id = (uint16_t)(u & 0xFFFFu);
    *out_gen = (uint16_t)((u >> 16) & 0xFFFFu);
}

// OS 后端触发时回调此函数（posix shim / win32 队列回调均指向它）。
// slot 校验、计数递减、auto-free 全在这里，平台无关。
void prts_timer_on_fire(int packed_tag)
{
    prts_timer_t *tm = g_prts_tm_singleton;
    if (!tm) {
        return;
    }

    uint16_t id = 0, gen16 = 0;
    unpack_sigval_u32(packed_tag, &id, &gen16);
    if (id == 0 || id > PRTS_TIMER_MAX) {
        return;
    }

    prts_timer_cb cb = NULL;
    void *userdata = NULL;

    pthread_mutex_lock(&tm->mtx);
    prts_timer_slot_t *s = &tm->slots[id];

    // gen 只比较低 16bit（与 sival_int 一致）
    if (!tm->inited || !s->active || ((uint16_t)(s->gen & 0xFFFFu) != gen16)) {
        pthread_mutex_unlock(&tm->mtx);
        return;
    }

    bool is_last = false;

    // 计数：先递减，确保在 cancel/destroy 之后的排队回调能因 gen mismatch 直接返回
    if (s->remaining > 0) {
        s->remaining--;
        if (s->remaining == 0) {
            // auto free：到期后删除 timer 并回收 slot
            os_timer_disarm(s->t);
            s->active = false;
            s->gen++; // 使旧回调线程失效（handle/sigval 均 mismatch）
            if (tm->free_top < PRTS_TIMER_MAX) {
                tm->free_ids[tm->free_top++] = (uint16_t)id;
            }
            is_last = true;
        }
    }

    // capture callback before unlock
    cb = s->cb;
    userdata = s->userdata;
    pthread_mutex_unlock(&tm->mtx);

    if (cb) {
        cb(userdata, is_last);
    }
}

int prts_timer_init(prts_timer_t *tm)
{
    if (!tm) return -EINVAL;

    // 单实例约束：已存在其他实例则拒绝
    if (g_prts_tm_singleton && g_prts_tm_singleton != tm) {
        return -EBUSY;
    }

    if (!tm->mtx_inited) {
        memset(tm, 0, sizeof(*tm));
        int ret = pthread_mutex_init(&tm->mtx, NULL);
        if (ret != 0) return -ret;
        tm->mtx_inited = true;
    }

    pthread_mutex_lock(&tm->mtx);
    tm->free_top = 0;
    for (uint16_t i = 1; i <= PRTS_TIMER_MAX; i++) {
        tm->slots[i].active = false;
        tm->slots[i].gen = 1; // 从 1 开始，避免 0 作为特殊值
        tm->free_ids[tm->free_top++] = i;
    }
    tm->inited = true;
    g_prts_tm_singleton = tm;
    pthread_mutex_unlock(&tm->mtx);
    log_info("==> PRTS Timer Initialized!");
    return 0;
}

static int prts_timer_cancel_locked(prts_timer_t *tm, uint32_t id, uint32_t gen)
{
    if (!tm || id == 0 || id > PRTS_TIMER_MAX) return -EINVAL;

    prts_timer_slot_t *s = &tm->slots[id];
    if (!tm->inited || !s->active || s->gen != gen) {
        return 0; // safe no-op
    }

    // stop future triggers
    os_timer_disarm(s->t);

    s->active = false;
    s->gen++; // invalidate outstanding callbacks/handles
    if (tm->free_top < PRTS_TIMER_MAX) {
        tm->free_ids[tm->free_top++] = (uint16_t)id;
    }
    return 0;
}

int prts_timer_cancel(prts_timer_handle_t handle)
{
    prts_timer_t *tm = g_prts_tm_singleton;
    if (!tm) return -EINVAL;

    uint32_t id = handle_id(handle);
    uint32_t gen = handle_gen(handle);
    if (id == 0 || id > PRTS_TIMER_MAX) return 0; // safe no-op

    pthread_mutex_lock(&tm->mtx);
    int ret = prts_timer_cancel_locked(tm, id, gen);
    pthread_mutex_unlock(&tm->mtx);
    return ret;
}

int prts_timer_destroy(prts_timer_t *tm)
{
    if (!tm) return -EINVAL;

    // destroy 立即返回语义：不等待正在执行的回调线程
    pthread_mutex_lock(&tm->mtx);
    if (!tm->inited) {
        pthread_mutex_unlock(&tm->mtx);
        return 0;
    }

    // cancel all active timers
    for (uint32_t id = 1; id <= PRTS_TIMER_MAX; id++) {
        prts_timer_slot_t *s = &tm->slots[id];
        if (s->active) {
            os_timer_disarm(s->t);
            s->active = false;
            s->gen++;
        }
    }

    tm->inited = false;
    // 保持 tm 内存由用户管理；trampoline 看到 singleton==NULL 会直接返回
    if (g_prts_tm_singleton == tm) {
        g_prts_tm_singleton = NULL;
    }
    pthread_mutex_unlock(&tm->mtx);
    return 0;
}

int prts_timer_create(prts_timer_handle_t *out,
                      uint64_t start_delay_us,
                      uint64_t interval_us,
                      int64_t fire_count,
                      prts_timer_cb cb,
                      void *userdata)
{
    prts_timer_t *tm = g_prts_tm_singleton;
    if (!tm || !out || !cb) return -EINVAL;
    if (fire_count == 0) return -EINVAL;
    if (fire_count < -1) return -EINVAL;
    if (fire_count == -1 && interval_us == 0) return -EINVAL;
    if (fire_count > 1 && interval_us == 0) return -EINVAL;

    pthread_mutex_lock(&tm->mtx);
    if (!tm->inited) {
        pthread_mutex_unlock(&tm->mtx);
        return -EINVAL;
    }
    if (g_prts_tm_singleton != tm) {
        pthread_mutex_unlock(&tm->mtx);
        return -EBUSY;
    }
    if (tm->free_top == 0) {
        pthread_mutex_unlock(&tm->mtx);
        return -ENOMEM;
    }

    uint16_t id16 = tm->free_ids[--tm->free_top];
    prts_timer_slot_t *s = &tm->slots[id16];

    // 生成新的 gen（确保低 16bit 变化，避免 sival_int 复用导致旧线程误命中）
    s->gen++;
    if ((uint16_t)(s->gen & 0xFFFFu) == 0) {
        s->gen++; // 避免低16位为0
    }

    s->cb = cb;
    s->userdata = userdata;
    s->interval_us = interval_us;
    s->remaining = fire_count;
    s->active = true;

    uint64_t first_us = start_delay_us;
    if (first_us == 0) {
        first_us = (interval_us != 0) ? interval_us : 1;
    }
    // one-shot（fire_count==1）不设周期
    uint64_t period_us = (fire_count == 1) ? 0 : interval_us;
    int packed = pack_sigval_u32(id16, (uint16_t)(s->gen & 0xFFFFu));

    int rc = os_timer_arm(&s->t, first_us, period_us, packed);
    if (rc != 0) {
        // 回收 slot
        s->active = false;
        s->gen++;
        tm->free_ids[tm->free_top++] = id16;
        pthread_mutex_unlock(&tm->mtx);
        log_error("prts_timer_create: os_timer_arm failed: %s(%d)", strerror(-rc), -rc);
        return rc;
    }

    *out = (((uint64_t)s->gen) << 32) | (uint64_t)id16;
    pthread_mutex_unlock(&tm->mtx);
    return 0;
}

