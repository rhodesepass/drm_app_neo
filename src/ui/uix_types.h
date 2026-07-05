#pragma once
//
// UIX（外部交互会话）协议类型 —— 独立小头，UI 侧(sim 也编)与 IPC 侧共用。
// 不要引入 apps/prts 等设备侧头。字段布局即线上格式，改动要同步：
// c_example/lib/ipc_common.h 与 usb_aio_handler/src/ui_ipc.c。
//
#include <stdint.h>

#define UIX_TITLE_MAX 64   // == UI_WARNING_MAX_TITLE_LENGTH
#define UIX_DESC_MAX 128   // == UI_WARNING_MAX_DESC_LENGTH

typedef enum {
    UIX_PENDING = 0,
    UIX_CONFIRMED = 1,   // 确认框选"是"，或选择框选中一项（见 choice）
    UIX_DENIED = 2,      // 用户选"否"/取消
    UIX_CANCELLED = 3,   // 发起方 SESSION_CANCEL 撤回
    UIX_TIMEOUT = 4,     // timeout_ms 到期无人操作
    UIX_NOT_FOUND = 5,   // session_id 不匹配（会话已被顶掉/不存在）
} uix_state_t;

typedef enum {
    UIX_USB_CHOICE_MTP = 0,
    UIX_USB_CHOICE_EPASS = 1,
    UIX_USB_CHOICE_FIDO = 2,
    UIX_USB_CHOICE_CHARGE_ONLY = 3,
} uix_usb_choice_t;

// 外部确认（是/否弹窗）- 请求数据。
typedef struct {
    char title[UIX_TITLE_MAX];
    char desc[UIX_DESC_MAX];
    uint32_t timeout_ms;  // 0 = 不超时（发起方自管）
} ipc_req_uix_confirm_start_data_t;

// USB 功能选择 - 请求数据。bit0=MTP bit1=EPASS bit2=FIDO bit3=仅充电
typedef struct {
    uint32_t func_mask;
    uint32_t timeout_ms;
} ipc_req_uix_usb_select_start_data_t;

// 两个 START 的响应数据。
typedef struct {
    uint32_t session_id;
} ipc_resp_uix_session_start_data_t;

// POLL / CANCEL - 请求数据。
typedef struct {
    uint32_t session_id;
} ipc_req_uix_session_data_t;

// POLL - 响应数据。
typedef struct {
    uint32_t state;   // uix_state_t
    uint32_t choice;  // 仅 USB_SELECT 且 CONFIRMED 时有效（uix_usb_choice_t）
} ipc_resp_uix_session_poll_data_t;
