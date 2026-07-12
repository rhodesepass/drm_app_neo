# IPC 接口参考

进程外部（如 `usb_aio_handler`、其他 app）与本进程之间的控制通道。核心代码：

- `src/apps/ipc_server.c` / `ipc_server.h`：传输层，epoll 服务线程
- `src/apps/ipc_common.h` / `ipc_common.c`：协议定义（请求/响应结构体、长度表、错误码）
- `src/apps/ipc_handler.c` / `ipc_handler.h`：请求分发与业务处理
- `src/ui/ipc_helper.h` / `ipc_helper.c`：IPC 线程 → LVGL 线程的转发队列
- `src/ui/uix_session.h` / `uix_session.c`：UIX 会话（轮询式弹窗）状态机
- `src/ui/uix_types.h`：UIX 协议类型（独立小头，sim 也编）

## 传输层

- Unix domain socket，`SOCK_SEQPACKET`（保留消息边界，不用像 `SOCK_STREAM` 那样自己处理粘包）
- 路径：`APPS_IPC_SOCKET_PATH` = `/tmp/epass_drm_app.sock`（`src/config.h`）
- 单线程 epoll 服务（`apps_ipc_server_thread`），accept + 收发都在一个线程里，每个请求同步处理完立即 `send` 回包
  - 没有并发写队列：注释里承认这是简化版，严格做法要 `EPOLLOUT` + per-connection 发送队列，低 QPS 下够用
- 单条消息上限：`APPS_IPC_MAX_MSG` = 512 字节
  - `recvmsg` + `MSG_TRUNC` 探测超限包，超限直接丢弃并回 `IPC_RESP_ERROR_MSG_TOO_LONG`
- `listen` backlog：`APPS_IPC_BACKLOG` = 16

## 消息格式

定长 tag-union，不是变长序列化协议。

```c
typedef struct {
    ipc_req_type_t type;
    union { /* 按 type 选择的请求数据结构体 */ };
} ipc_req_t;

typedef struct {
    ipc_resp_type_t type;
    union { /* 按 type 选择的响应数据结构体 */ };
} ipc_resp_t;
```

- `calculate_ipc_req_size(type)` / `calculate_ipc_resp_size_by_req(type)`：按 type 查表返回该 union 分支应有的字节数
- `apps_ipc_handler()` 收到包后先比对实际长度和期望长度，不匹配直接回 `IPC_RESP_ERROR_LENGTH_MISMATCH`，不进入 switch
- 协议里没有版本号/CRC，纯靠定长校验

### 错误码（`ipc_resp_type_t`）

| 值 | 含义 |
|---|---|
| `IPC_RESP_OK` | 成功 |
| `IPC_RESP_ERROR_MSG_TOO_LONG` | 请求包超过 `APPS_IPC_MAX_MSG`，被截断丢弃 |
| `IPC_RESP_ERROR_NOMEM` | 服务端 malloc 失败 |
| `IPC_RESP_ERROR_INVALID_REQUEST` | 参数非法（索引越界、子系统未初始化等） |
| `IPC_RESP_ERROR_STATE_CONFLICT` | UIX 会话已存在，不允许并发发起第二个 |
| `IPC_RESP_ERROR_LENGTH_MISMATCH` | 收到包长度与该 type 期望长度不符 |
| `IPC_RESP_ERROR_UNKNOWN` | 未知请求类型 |

## 操作清单（`IPC_REQ_MAX = 20`）

按子模块分组，枚举值见 `src/apps/ipc_common.h`。

### UI 子模块

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 0 | `IPC_REQ_UI_WARNING` | title/desc/icon/color | 空 | 直接调 `ui_warning_custom`，同步执行，不经 helper |
| 1 | `IPC_REQ_UI_GET_CURRENT_SCREEN` | 空 | `screen` | 同步读 |
| 2 | `IPC_REQ_UI_SET_CURRENT_SCREEN` | `screen` | 空 | **经 `ui_ipc_helper` 转发到 LVGL 线程执行** |
| 3 | `IPC_REQ_UI_FORCE_DISPIMG` | `path[128]` | 空 | 强制设置扩列图，不校验路径，**经 helper** |

### PRTS 子模块

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 4 | `IPC_REQ_PRTS_GET_STATUS` | 空 | state/operator_count/operator_index | 同步读 |
| 5 | `IPC_REQ_PRTS_SET_OPERATOR` | operator_index | 空 | 校验索引范围，调 `prts_request_set_operator` |
| 6 | `IPC_REQ_PRTS_GET_OPERATOR_INFO` | operator_index | name/uuid/description/icon_path/source | 校验索引范围 |
| 7 | `IPC_REQ_PRTS_SET_BLOCKED_AUTO_SWITCH` | is_blocked | 空 | 原子写 `prts->is_auto_switch_blocked` |
| 15 | `IPC_REQ_PRTS_RELOAD_ASSETS` | 空 | 空 | 先经 helper 切到 `SCREEN_SPINNER`，再 `prts_request_reload_assets` |

### Settings 子模块

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 8 | `IPC_REQ_SETTINGS_GET` | 空 | brightness/switch_interval/switch_mode/usb_mode/ctrl_word | 加锁读 `g_settings` |
| 9 | `IPC_REQ_SETTINGS_SET` | 同上 | 空 | 加锁写 + 落盘；`usb_mode` 字段仅保留占位落盘，**已不再触发 usbctl**（USB 归 `usb_aio_handler` 接管） |

### MediaPlayer 子模块

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 10 | `IPC_REQ_MEDIAPLAYER_GET_VIDEO_PATH` | 空 | path | 需 `g_mediaplayer.drm_warpper` 已初始化，否则 `INVALID_REQUEST` |
| 11 | `IPC_REQ_MEDIAPLAYER_SET_VIDEO_PATH` | path | 空 | 直接 stop + play，同步生效（非排期） |

### Overlay 子模块

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 12 | `IPC_REQ_OVERLAY_SCHEDULE_TRANSITION` | duration/type/image_path/bg_color | 空 | 纯过渡，不换视频 |
| 13 | `IPC_REQ_OVERLAY_SCHEDULE_TRANSITION_VIDEO` | 上面 + video_path | 空 | 带视频切换的过渡 |

两者都先 `overlay_abort()` 打断在途操作，再 malloc `oltr_params_t` + `oltr_callback_t`（+ 视频版多一个 `middle_cb` 用户数据）异步跑过渡状态机；`type` 支持 `TRANSITION_TYPE_FADE / MOVE / SWIPE`，非法类型回 `INVALID_REQUEST`。资源在 `end_cb`（`overlay_transition_end_cb_free_image`）里统一释放，视频版额外在过渡中途由 `middle_cb`（`overlay_transition_middle_cb_video`）切视频。

### 全局

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 14 | `IPC_REQ_APP_EXIT` | exit_code | 空 | 置 `g_exitcode` + `g_running=0`，退出主循环 |

### UIX 子模块（外部交互会话，`usb_aio_handler` 用）

轮询式：`*_START` 立即返回 `session_id`，不等用户操作 —— handler 跑在 IPC 线程，不能阻塞，而操作结果产生在 LVGL 线程。调用方需自行拿 `session_id` 轮询 `SESSION_POLL`。**同时只允许一个会话**，冲突直接回 `IPC_RESP_ERROR_STATE_CONFLICT`。

| # | 请求 | 请求数据 | 响应数据 | 说明 |
|---|---|---|---|---|
| 16 | `IPC_REQ_UIX_CONFIRM_START` | title/desc/timeout_ms | session_id | 是/否弹窗 |
| 17 | `IPC_REQ_UIX_USB_SELECT_START` | func_mask（位图）/timeout_ms | session_id | USB 功能选择弹窗 |
| 18 | `IPC_REQ_UIX_SESSION_POLL` | session_id | state/choice | 查会话状态 |
| 19 | `IPC_REQ_UIX_SESSION_CANCEL` | session_id | 空 | 撤回会话；若确有会话被取消，经 helper 摘掉屏上弹窗 |

`uix_state_t`（`src/ui/uix_types.h`）：

| 值 | 含义 |
|---|---|
| `UIX_PENDING` | 等待用户操作 |
| `UIX_CONFIRMED` | 确认框选“是”，或选择框选中一项（结果见 `choice`） |
| `UIX_DENIED` | 用户选“否”/取消 |
| `UIX_CANCELLED` | 发起方 `SESSION_CANCEL` 主动撤回 |
| `UIX_TIMEOUT` | `timeout_ms` 到期无人操作 |
| `UIX_NOT_FOUND` | `session_id` 不匹配（会话已被顶掉或不存在） |

`func_mask` 位定义（`uix_usb_choice_t`）：bit0 = MTP，bit1 = EPASS，bit2 = FIDO，bit3 = 仅充电。

## 两个跨线程机制

### 1) `ui_ipc_helper`：IPC 线程 → LVGL 线程

LVGL 不是线程安全的，凡是要动 UI 状态的请求（切屏、强制扩列图、UIX 弹窗/撤回）不能在 IPC 线程里直接调 LVGL API。做法是 malloc 一个 `ui_ipc_helper_req_t`，`ui_ipc_helper_request()` 投递给 LVGL 线程侧处理——类似中断里不能直接干活，得往任务队列 post 一个工作项，回到主循环里再执行。

`ui_ipc_helper_req_type_t`（`src/ui/ipc_helper.h`）：

| 值 | 用途 |
|---|---|
| `SET_CURRENT_SCREEN` | 切屏 |
| `FORCE_DISPIMG` | 强制扩列图路径 |
| `REFRESH_OPLIST` | 刷新 oplist（目前没有对应的 IPC 请求触发，是内部用途） |
| `UIX_SHOW` | 弹出 UIX 交互屏（参数从 `uix_session` 读） |
| `UIX_DISMISS` | 撤回 UIX 交互屏 |

### 2) UIX 会话状态机

见上文“UIX 子模块”一节；本质是把“阻塞式确认”拆成“发起（写状态）+ 轮询（读状态）”两步，绕开 IPC handler 不能阻塞、结果又只能在 LVGL 线程产生的限制。

## 外部耦合点（改协议前必读）

`src/ui/uix_types.h` 头部注释明确：UIX 协议的字段布局是线上格式，仓库外还有两处需要同步：

- `c_example/lib/ipc_common.h`
- `usb_aio_handler/src/ui_ipc.c`

这两处不在本仓库。改动 `IPC_REQ_UIX_*` 或 `uix_*` 相关结构体时必须同步过去，否则设备侧 `usb_aio_handler` 和这边的定长协议对不齐，会直接触发 `IPC_RESP_ERROR_LENGTH_MISMATCH`。

同理，新增/修改任意 IPC 请求类型时，三处要一起改：

1. `src/apps/ipc_common.h`：加枚举值 + 请求/响应结构体，塞进 `ipc_req_t`/`ipc_resp_t` 的 union
2. `src/apps/ipc_common.c`：`calculate_ipc_req_size` / `calculate_ipc_resp_size_by_req` / （非 release 下）`ipc_print_req_type` 加对应分支
3. `src/apps/ipc_handler.c`：写 handler 函数 + 在 `apps_ipc_handler()` 的 switch 里挂上
