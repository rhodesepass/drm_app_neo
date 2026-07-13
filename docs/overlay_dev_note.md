

# Overlay层 开发指南

## 基本约束

overlay 层用于在 VIDEO 之上 UI之下绘制“过渡动画（transition）”与“干员信息（opinfo）”等覆盖效果。核心约束是：**PRTS 的 timer 回调线程必须尽快返回**，因此任何可能耗时的逐帧绘制都要放到 overlay worker 线程执行。

### 1) 线程与时序模型

- `overlay_t` 内部维护：
  - overlay 图层双缓冲（`overlay_buf_1/2`）以及对应 display queue item
  - 一个专用线程 `overlay_worker_thread`，通过 `overlay_worker_schedule()` 投递任务
  - `overlay->request_abort`：请求提前终止（例如效果未完成但要立刻停止）
  - `overlay->overlay_timer_handle`：**worker 侧清理完成的同步信号**
- `overlay_worker_schedule(overlay, func, userdata)`：
  - worker 空闲：提交任务（`func(userdata, skipped_frames)`）
  - worker 忙：丢弃这次任务并累计 `skipped_frames`（下一次执行时扣减）
- `overlay_abort(overlay)`：
  - 设置 `overlay->request_abort = 1`
  - 轮询等待 `overlay->overlay_timer_handle == 0`
  - 语义上等价于：“请求终止 overlay 效果，并等待 worker 完成资源回收/注销定时器”

> 结论：**只要你的效果创建了 `overlay->overlay_timer_handle`（也就是“需要 worker”的类型），你就必须保证在 worker 中把资源回收干净，并把 `overlay->overlay_timer_handle` 归零**，否则 `overlay_abort()` 会一直等。

### 2) 新增 Transition（过渡效果）怎么做

#### 2.1 实现与配置落点

- 实现文件：
  - `src/overlay/transitions.h`：新增类型/参数/函数声明
  - `src/overlay/transitions.c`：实现绘制与动画驱动
- 配置映射
  - `src/prts/operators.c`：把 `transition_in/transition_loop` 的字符串映射到 `transition_type_t`，并校验/填充 `oltr_params_t`

#### 2.2 不需要 worker 的 Transition（一次性绘制 + layer_animation 驱动）

适用场景：准备阶段可一次性绘制完成，运行阶段只靠 `layer_animation_*` 控制 alpha/坐标，无需逐帧重画。现有参考：`overlay_transition_fade()`、`overlay_transition_move()`。

实现步骤：

- 在入口函数内先 `overlay->request_abort = 0`
- 设置图层初始状态（例如 alpha/coord）
- `drm_warpper_dequeue_free_item()` 取一块 free buffer
- 在该 buffer 上完成一次性绘制（背景色、可选图片等）
- `drm_warpper_enqueue_display_item()` 提交显示
- 调用 `layer_animation_*` 启动动画
- 如需“中间点”做一次回调（例如遮住后挂载 video），用一次性 `prts_timer_create(..., count=1, middle_cb)` 即可

注意点：

- 这种模式通常 **不占用** `overlay->overlay_timer_handle`（例如 fade/move 的 middle timer 是局部 handle），因此也不依赖 `overlay_abort()` 的等待逻辑。

#### 2.3 需要 worker 的 Transition（timer 只 schedule，逐帧绘制在 worker）

适用场景：需要逐帧绘制。现有参考：`overlay_transition_swipe()`。

关键规则：

- **资源申请时机**：在“效果入口函数”（非 timer 回调）里申请/准备 worker 需要的资源（例如 `malloc`、贝塞尔表预计算、双缓冲清空/挂载）。不要在 timer 回调里做耗时准备。
- **驱动方式**：
  - `prts_timer_create(&overlay->overlay_timer_handle, ..., cb=timer_cb, userdata=data)`
  - `timer_cb` 里只做：`overlay_worker_schedule(overlay, worker_func, data)`
  - 真正的绘制/状态机推进在 `worker_func` 中完成
- **request_abort 处理位置**：必须在 `worker_func` 的开头检查 `overlay->request_abort`，为真则执行清理并立即返回。
- **资源回收位置**：必须在 worker 中回收（见下节原因），清理完成后必须把 `overlay->overlay_timer_handle = 0`。

### 3) OpInfo（干员信息效果）：统一元素引擎

opinfo 只有一个绘制后端：**元素引擎**（`src/overlay/opinfo.c`）。三种 `overlay.type`
都在解析阶段被翻译成一张"元素列表"（`olopinfo_element_t[]`），由同一个 worker 状态机驱动：

- `image`     → 1 个静态 image 元素（`overlay_opinfo_build_image_elements`）
- `arknights` → 预设元素列表（`overlay_opinfo_build_arknights_elements`，图片来自 cacheassets）
- `custom`    → `overlay.options.elements` 的直接映射（用户自定义，见 3.3）

#### 3.1 引擎工作原理

- **影子缓冲**：所有元素先画进堆上的全尺寸 ARGB 影子缓冲（cached 内存，始终持有当前帧
  完整画面），再按脏区一次 `fbdraw_copy_rect` 落盘显存。半透明混合的读操作全部发生在
  cached 内存，uncached 显存零回读。
- **重叠组（rect fight 的解法）**：show 时按元素 bbox 求重叠连通分量。组内任一成员动画
  推进时，整组先把成员区域清为透明，再按元素列表顺序（z 序）以"当前动画状态"重画，
  最后组 bbox 并集一次落盘。因此：
  - fade 元素反复混合不会累积（每次从透明重画）
  - 重叠区域每像素每帧只写一次显存，扫描线抓不到中间态（推广了旧实现中
    fade 三角 + logo 手工合成的做法）
  - 元素的遮挡关系由**列表顺序**决定：后面的元素画在上面
- **独占打字机快路径**：不与任何元素重叠的 typewriter 文本走增量绘制（每步只画新增
  codepoint），避免整段文本逐帧重排。
- **生命周期**：引擎控制块是 static（timer 回调线程与 worker 线程都会引用它，而
  `prts_timer_cancel` 不等在途回调结束）；影子缓冲和元素运行态数组在堆上，由 worker
  在"正常结束 / request_abort"两条路径里回收并把 `overlay_timer_handle` 归零。
  含 scroll 元素的列表（如 arknights 的箭头）永不自然结束，跑到 abort 为止。

#### 3.2 元素与动画参考

元素类型（坐标/尺寸/字号全部为 360x640 基准，绘制时套 `S()`）：

| type | 说明 | 关键字段 |
|---|---|---|
| `text` | 水平文本，支持 `\n` 多行 | `text` `font` `font_size` `line_height` `letter_space` `color`；`w/h` 缺省=宽到屏幕右缘、高按行数 |
| `text_rot90` | 顺时针旋转 90° 的文本 | 同上 + `faux_bold`（+1px 重画加粗）、`bold_split`（空格前加粗空格后常规）；`w/h` 必填 |
| `image` | 图片，按原生尺寸绘制 | `image`（相对干员目录的路径） |
| `rect` | 纯色矩形 | `w/h` 必填、`color`；`border_width`>0 时画空心边框（线宽，360 基准） |
| `barcode` | code128 条形码（旋转 90°带文字） | `text` 必填；`w/h` 缺省 50x180 |
| `corner_fade` | 右下角渐变三角（位置固定右下） | `color`；`w`=目标半径（缺省 192） |

动画（`animation`，缺省 `none`；`start_frame` 以 30fps 帧计；`speed` 语义随动画变化）：

| animation | 适用 | speed 含义（缺省值） |
|---|---|---|
| `none` | 全部 | —（`start_frame` 时一次画出） |
| `typewriter` | text | 每 codepoint 帧数（3） |
| `scramble` | text | 每 codepoint 帧数（2）；未稳定字符随机跳变、逐个稳定成真实文本（CJK 替换为 ASCII，宽度抖动是效果的一部分） |
| `eink` | image/rect/barcode | 每闪烁态帧数（15）；白→黑→白→(停顿)→内容 |
| `fade` | image/text/rect | 每帧不透明度增量（8） |
| `wipe` | rect/image | 划入总帧数（40），cubic-bezier(0.42,0,0.58,1)；`direction: ltr/rtl/ttb/btt`（缺省 ltr）定方向 |
| `move` | corner_fade 以外 | 滑入总帧数（15）；从落点 + `from_dx/from_dy` 偏移处按贝塞尔滑入 |
| `scroll` | image | 每帧滚动像素（1），垂直循环、永不结束 |
| `blink` | corner_fade 以外 | 半周期帧数（15）；周期显示/隐藏、永不结束 |
| `grow` | corner_fade | 每帧半径增量（10） |

通用字段：

- `anchor`（缺省 `tl`）：`tl/tr/bl/br`，右/下锚定时 `x/y` 是元素相应边到屏幕边缘的距离，
  让"贴右下角"不依赖图片尺寸（如 arknights 的 logo：`anchor=br, x=10, y=10`）
- `end_frame`（缺省 0=永不）：到该帧隐藏元素并视为播放完毕。是"临时字幕/阶段提示"的
  基础；也让 `scroll/blink` 这类循环动画可以自然收尾（否则引擎跑到 overlay abort 为止）
- `move` 的运动路径（起点∪终点矩形）整体参与重叠分组，路径扫过其它元素时二者会被
  归入同组逐帧重组——运动范围越大，重绘代价越大，路径别横穿大量元素

#### 3.3 custom 类型配置示例（epconfig.json）

```json
"overlay": {
    "type": "custom",
    "options": {
        "appear_time": 4000000,
        "duration": 1000000,
        "elements": [
            { "type": "corner_fade", "color": "#8B0000",
              "animation": "grow", "start_frame": 15 },
            { "type": "text", "x": 70, "y": 415, "text": "AMIYA",
              "font": "display", "font_size": 40, "color": "#FFFFFF",
              "animation": "typewriter", "start_frame": 30, "speed": 3 },
            { "type": "rect", "x": 70, "y": 455, "w": 280, "h": 1,
              "animation": "wipe", "start_frame": 80, "speed": 40 },
            { "type": "image", "image": "logo.png", "anchor": "br", "x": 10, "y": 10,
              "animation": "fade", "start_frame": 30 },
            { "type": "barcode", "x": 1, "y": 450, "w": 50, "h": 180,
              "text": "OPERATOR - CUSTOM", "animation": "eink", "start_frame": 30 },
            { "type": "text", "x": 70, "y": 480, "text": "ID // 042-C7#F9",
              "font": "display", "font_size": 14,
              "animation": "scramble", "start_frame": 40 },
            { "type": "rect", "x": 60, "y": 405, "w": 240, "h": 60, "border_width": 1,
              "animation": "move", "from_dx": -80, "from_dy": 0,
              "start_frame": 20, "speed": 15 },
            { "type": "rect", "x": 330, "y": 20, "w": 8, "h": 8, "color": "#FF3B30",
              "animation": "blink", "start_frame": 60, "speed": 10 },
            { "type": "text", "x": 70, "y": 60, "text": "ESTABLISHING LINK...",
              "animation": "typewriter", "start_frame": 0, "end_frame": 90 }
        ]
    }
}
```

- `appear_time`（必填>0）：loop 过渡结束后多久显示；`duration`（可选）：进场滑入时长(us)，缺省 1s
- 元素数量上限 `OPINFO_ELEMENTS_MAX`(24)，超出截断
- `font`: `body`(思源黑) / `title`(思源宋 Heavy) / `display`(Bebas) / `icon`(FontAwesome)
- 校验规则见 `src/prts/operators.c` 的 `parse_custom_element`：`type` 非法/关键字段缺失报
  ERROR（整个干员解析失败）；`animation` 与 `type` 不匹配、`anchor`/`font` 非法等回退默认并 WARN

#### 3.4 新增元素类型 / 动画怎么做

- `src/overlay/opinfo.h`：`opinfo_element_type_t` / `opinfo_anim_t` 加枚举与字段
- `src/overlay/opinfo.c`：
  - `el_resolve()`：解析 bbox（决定重叠分组与清除范围，**绘制必须落在 bbox 内**）
  - `el_advance()`：推进每帧状态、标 dirty，结束置 done（永不结束的动画要置 `has_loop`）
  - `el_draw()`：按"当前状态"**从头重画**（组重组的前提，不能依赖上一帧画面）
- `src/prts/operators.c`：`parse_custom_element` 的映射表与校验（含 `anim_valid_for_type`）
- arknights 预设要用新能力的话改 `overlay_opinfo_build_arknights_elements`

### 4) 目前Overlay 编程模型的设计考量

#### 4.1 为什么资源必须在 worker 中回收？

timer虽然给出了is_last字段用来处理释放问题，但是timer 回调线程与 overlay worker 线程是并发的。如果你在 timer 回调（或其它线程）里 free 了 worker 正在访问的数据，就会出现 **UAF（Use-After-Free）**。

#### 4.2 必须保证 `overlay_timer_handle` 能归零（否则 stop 会卡死）

只要你的效果创建了 `overlay->overlay_timer_handle`：

- **正常结束路径**：最后一帧/结束条件满足时，在 worker 内 cancel timer + 回收资源 + `overlay_timer_handle=0`
- **abort 路径**：检测到 `request_abort` 时，在 worker 内 cancel timer + 回收资源 + `overlay_timer_handle=0`

否则 `overlay_abort()` 会一直阻塞等待（轮询 `overlay_timer_handle`）。

### 5) PC Target（完整应用跑在 PC）

不是仿真——**同一套 main.c → prts → mediaplayer → overlay → LVGL UI → apps/IPC
完整链路**编译成 PC 可执行，三个后端缝在链接期切换（共享代码零 `#ifdef` 散弹）：

| 缝 | 设备 | PC |
|---|---|---|
| 显示 | `src/driver/drm_warpper.c`（libdrm atomic + DEBE） | `src/driver/drm_warpper_sdl.c`（SDL 合成线程，三层软件混叠） |
| 视频 | `src/render/mediaplayer.c`（自制 demux/DPB + cedrus V4L2） | `src/render/mediaplayer_ffmpeg.c`（libavformat/avcodec/swscale，planar NV12） |
| 按键 | `src/driver/key_enc_evdev.c`（/dev/input） | `src/driver/key_sdl.c`（SDL 键盘：←→/Enter/Esc/End） |

字体：PC 走 **fontconfig** 按 family 解析系统字体（`Source Han Sans SC` 等），
未精确命中时回退可执行同级 `res/fonts` 自带的四款（观感与设备一致）。

```bash
# 依赖：sdl2 + ffmpeg(libavformat/avcodec/avutil/swscale) + fontconfig
#      + freetype2 + libpng + libdrm(仅头文件)。缺依赖时 PC target 自动跳过。
cmake -B build && cmake --build build --target app_pc_360 app_pc_720 -j
cd build && ./app_pc_360
```

- **数据目录**：`./pcdata`（可 `-DEPASS_PC_DATA_DIR=` 改），布局与设备根文件系统
  同构：`pcdata/assets/<干员>/epconfig.json`（screen 填 `360x640`/`720x1280`，
  素材尺寸约束与设备相同：视频必须命中三档 coded 尺寸白名单）、`pcdata/app/`、
  `pcdata/dispimg/`、`pcdata/epass_cfg.bin`。无干员时走 res/fallback。
- **无头验证 / 脚本驱动**（`SDL_VIDEODRIVER=dummy` 下可全跑）：
  ```bash
  EPASS_SHOT=/tmp/shot.bmp EPASS_SHOT_MS=5000   # 到点截图并正常退出（顺带验证销毁链）
  EPASS_AUTOKEY="4500:enter,6000:esc"           # 到点注入按键（left/right/enter/esc/end）
  SIM_WIN_X/SIM_WIN_Y                            # 窗口位置
  ```
- **已知差异**：合成线程 ~60Hz 轮询，无真机 vblank/扫描语义（撕裂窗口同类不同相）；
  FLIP 帧翻页即拷贝进纹理、item 立即回流（"在屏押 0 格"，宽松于真机的 1 格）；
  退出码（关机/重启/前台 app 接力）在 PC 上没有外部 launcher，等同直接退出；
  `usbctl/usbaioctl` 等设备工具的调用被屏蔽；电池 ADC/背光/SD 检测因路径不存在
  自然落空。

## 开发示例

[opinfo.c](../src/overlay/opinfo.c) 的元素引擎（`engine_compose` / `el_advance` / `el_draw`）
是"需要 worker 的效果"的完整参考实现；`overlay_opinfo_build_arknights_elements` 展示了
如何把一个具体版式声明成元素列表。