#pragma once
//
// uix_session —— 外部交互会话的共享状态（IPC 线程写入请求 / LVGL 线程回填结果）。
// 现有 IPC→UI 只有单向 SPSC 队列，没有结果回传通道；这里补一个 mutex 小结构。
// 同时只有一个会话（usb_aio_handler 是唯一发起方，低频）。
//
#include "ui/uix_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UIX_KIND_NONE = 0,
    UIX_KIND_CONFIRM,
    UIX_KIND_USB_SELECT,
} uix_kind_t;

void uix_session_init(void);
void uix_session_destroy(void);

// IPC 线程：开会话。已有 PENDING 会话时返回 0（调用方回 STATE_CONFLICT）。
uint32_t uix_session_confirm_start(const ipc_req_uix_confirm_start_data_t *req);
uint32_t uix_session_usb_select_start(const ipc_req_uix_usb_select_start_data_t *req);

// IPC 线程：查询/撤销。id 不匹配返回 false（回 UIX_NOT_FOUND）。
bool uix_session_poll(uint32_t id, uint32_t *state, uint32_t *choice);
bool uix_session_cancel(uint32_t id);

// LVGL 线程：弹屏时读取当前会话参数（返回当前 kind；无会话 UIX_KIND_NONE）。
uix_kind_t uix_session_snapshot(uint32_t *id, char *title, size_t title_cap,
                                char *desc, size_t desc_cap, uint32_t *func_mask);

// LVGL 线程：用户操作/超时回填结果。id 不匹配（会话已被撤销/顶掉）则忽略。
void uix_session_finish(uint32_t id, uix_state_t state, uint32_t choice);

// LVGL 线程：周期检查超时。到期则置 UIX_TIMEOUT 并返回 true（调用方负责收屏）。
bool uix_session_tick(void);
