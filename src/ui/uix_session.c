#include "ui/uix_session.h"
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <utils/log.h>

static struct {
    pthread_mutex_t mtx;
    uint32_t next_id;     // 自增；0 保留为"无会话"
    uint32_t id;
    uix_kind_t kind;
    uix_state_t state;
    uint32_t choice;
    struct timespec deadline; // timeout_ms != 0 时有效
    bool has_deadline;
    char title[UIX_TITLE_MAX];
    char desc[UIX_DESC_MAX];
    uint32_t func_mask;
} S = { .mtx = PTHREAD_MUTEX_INITIALIZER, .next_id = 1 };

static void set_deadline(uint32_t timeout_ms) {
    if (timeout_ms == 0) {
        S.has_deadline = false;
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &S.deadline);
    S.deadline.tv_sec += timeout_ms / 1000;
    S.deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (S.deadline.tv_nsec >= 1000000000L) {
        S.deadline.tv_sec++;
        S.deadline.tv_nsec -= 1000000000L;
    }
    S.has_deadline = true;
}

void uix_session_init(void) {}
void uix_session_destroy(void) {}

static uint32_t start_locked(uix_kind_t kind, uint32_t timeout_ms) {
    if (S.kind != UIX_KIND_NONE && S.state == UIX_PENDING) {
        return 0;
    }
    S.id = S.next_id++;
    if (S.next_id == 0) S.next_id = 1;
    S.kind = kind;
    S.state = UIX_PENDING;
    S.choice = 0;
    set_deadline(timeout_ms);
    return S.id;
}

uint32_t uix_session_confirm_start(const ipc_req_uix_confirm_start_data_t *req) {
    pthread_mutex_lock(&S.mtx);
    uint32_t id = start_locked(UIX_KIND_CONFIRM, req->timeout_ms);
    if (id) {
        memcpy(S.title, req->title, sizeof(S.title));
        S.title[sizeof(S.title) - 1] = '\0';
        memcpy(S.desc, req->desc, sizeof(S.desc));
        S.desc[sizeof(S.desc) - 1] = '\0';
    }
    pthread_mutex_unlock(&S.mtx);
    return id;
}

uint32_t uix_session_usb_select_start(const ipc_req_uix_usb_select_start_data_t *req) {
    pthread_mutex_lock(&S.mtx);
    uint32_t id = start_locked(UIX_KIND_USB_SELECT, req->timeout_ms);
    if (id) {
        S.func_mask = req->func_mask;
    }
    pthread_mutex_unlock(&S.mtx);
    return id;
}

bool uix_session_poll(uint32_t id, uint32_t *state, uint32_t *choice) {
    pthread_mutex_lock(&S.mtx);
    bool found = (id != 0 && id == S.id && S.kind != UIX_KIND_NONE);
    if (found) {
        *state = (uint32_t)S.state;
        *choice = S.choice;
        // 终态被取走后释放会话槽，允许下一个会话
        if (S.state != UIX_PENDING) {
            S.kind = UIX_KIND_NONE;
            S.id = 0;
        }
    }
    pthread_mutex_unlock(&S.mtx);
    return found;
}

bool uix_session_cancel(uint32_t id) {
    pthread_mutex_lock(&S.mtx);
    bool found = (id != 0 && id == S.id && S.kind != UIX_KIND_NONE);
    if (found) {
        if (S.state == UIX_PENDING) S.state = UIX_CANCELLED;
        // 发起方已不关心结果，直接释放
        S.kind = UIX_KIND_NONE;
        S.id = 0;
    }
    pthread_mutex_unlock(&S.mtx);
    return found;
}

uix_kind_t uix_session_snapshot(uint32_t *id, char *title, size_t title_cap,
                                char *desc, size_t desc_cap, uint32_t *func_mask) {
    pthread_mutex_lock(&S.mtx);
    uix_kind_t kind = (S.state == UIX_PENDING) ? S.kind : UIX_KIND_NONE;
    if (kind != UIX_KIND_NONE) {
        if (id) *id = S.id;
        if (title && title_cap) {
            strncpy(title, S.title, title_cap - 1);
            title[title_cap - 1] = '\0';
        }
        if (desc && desc_cap) {
            strncpy(desc, S.desc, desc_cap - 1);
            desc[desc_cap - 1] = '\0';
        }
        if (func_mask) *func_mask = S.func_mask;
    }
    pthread_mutex_unlock(&S.mtx);
    return kind;
}

void uix_session_finish(uint32_t id, uix_state_t state, uint32_t choice) {
    pthread_mutex_lock(&S.mtx);
    if (id != 0 && id == S.id && S.state == UIX_PENDING) {
        S.state = state;
        S.choice = choice;
        log_info("uix: session %u finished, state=%d choice=%u", id, state, choice);
    }
    pthread_mutex_unlock(&S.mtx);
}

bool uix_session_tick(void) {
    bool expired = false;
    pthread_mutex_lock(&S.mtx);
    if (S.kind != UIX_KIND_NONE && S.state == UIX_PENDING && S.has_deadline) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > S.deadline.tv_sec ||
            (now.tv_sec == S.deadline.tv_sec && now.tv_nsec >= S.deadline.tv_nsec)) {
            S.state = UIX_TIMEOUT;
            expired = true;
            log_info("uix: session %u timed out", S.id);
        }
    }
    pthread_mutex_unlock(&S.mtx);
    return expired;
}
