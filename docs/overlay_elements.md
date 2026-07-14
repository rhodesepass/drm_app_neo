# Overlay 素材编写规约（opinfo 元素引擎）

面向**素材作者**：怎么在 `epconfig.json` 的 `overlay.type = "custom"` 下写 `overlay.options.elements[]`。
顺带把两件常被问到的事讲清：不同分辨率的坐标怎么算、这套动画的循环能力到哪为止。

- 引擎实现：`src/overlay/opinfo.c`
- JSON 解析/校验：`src/prts/operators.c`（`parse_custom_element`）
- 覆盖全部 type/animation 的真实示例：`testdata/opinfo_custom/epconfig.json`

三种 `overlay.type` 最终都被翻译成同一张「元素列表」，由同一个 worker 状态机驱动：

| type | 来源 |
|---|---|
| `image` | 1 个静态 image 元素 |
| `arknights` | 固件内置预设列表（`overlay_opinfo_build_arknights_elements`，素材不可编辑） |
| `custom` | 本文档描述的 `elements[]` 直接映射 |

---

## 1. 坐标与分辨率模型

### 1.1 单一基准 + 整数倍缩放

**所有几何量一律以 360×640 逻辑基准书写**，固件绘制时统一乘 `UI_SCALE`（宏 `S(x) = x * UI_SCALE`，`src/ui_metrics.h`）：

| 固件屏目标 | UI_SCALE |
|---|---|
| 360×640 | 1 |
| 720×1280 | 2 |

受此规则自动缩放的字段：`x` `y` `w` `h` `font_size` `line_height` `letter_space` `border_width` `from_dx` `from_dy`、`corner_fade` 的半径。

> 作者**永远写 360 基准整数**，绝不要自己乘 2 去「适配」720。这跟编译期常量一样，缩放在固件里定死，运行时零开销。

### 1.2 矢量 vs 位图：谁能无损放大

| 元素 | 尺寸来源 | 720 下表现 |
|---|---|---|
| `text` / `text_rot90` / `rect` / `barcode` / `corner_fade` | 全部走 `S()`，**矢量重画** | 物理尺寸精确 2×，边缘清晰 |
| `image`（用户位图） | 图片**原生像素**，不过 `S()` | 见 1.3 |

### 1.3 360 素材在 720 固件上跑

720 固件**允许**加载 `screen: "360x640"` 的旧素材包（360 固件只收 `"360x640"`）。此时：

- 文本 / 矩形 / 三角 / 条形码：坐标与字号全部 ×2 矢量重画，**清晰**。
- 用户位图（`image` 字段）：加载后做**最近邻 2× 放大**（`imgscale_upscale_nn_rgba`）。结果**尺寸、定位都正确，但画面是放大的马赛克**。

规约：
1. **写 360 基准，不手动缩放。**
2. `screen` 字段必须与素材实际分辨率一致。
3. **要 720 下位图也清晰，就单出一份 `screen: "720x1280"` 的素材包**（此时不放大，原图直贴）。文本/矢量元素无所谓，两档通用。

---

## 2. 全局约定

- **坐标基准**：360×640，见 §1。
- **帧率**：30fps，1 帧 = 33ms。所有 `start_frame` / `end_frame` / `speed` 以帧计。
- **z 序 = 数组顺序**：先写的在底层，后写的盖在上面。
- **重叠自动分组**：bbox 相交的元素归为一组，组内任一元素推进时整组「清区域 → 按 z 序重画 → 一次落盘」。所以半透明叠加、`fade` 反复混合都不会累积脏，作者不用关心合成，只管排 z 序。
- **上限**：
  - 元素数 ≤ **24**（`OPINFO_ELEMENTS_MAX`）。
  - `text` ≤ 255 字节（UTF-8，支持 `\n`）。
  - `image` 路径 ≤ 127 字节。
- **颜色**：`color` 只接受 `"#RRGGBB"`（7 字符含 `#`），**alpha 恒为不透明**。
  **素材侧无法直接指定半透明色**——半透明只能通过 `fade` 动画（运行时降不透明度）或 `corner_fade`（程序化渐变）获得。
- **图片来源**：custom 元素只能用 `image`（素材包内相对路径的用户图）。内置 cacheasset 是 `arknights` 预设专用，custom 里无法引用。

---

## 3. 元素类型（`type`，必填）

| type | 说明 | 必填字段 | 缺省行为 |
|---|---|---|---|
| `text` | 水平文本，支持 `\n` 多行 | `text` | `w=0`→宽到屏幕右缘；`h=0`→按行数×行高 |
| `text_rot90` | 顺时针旋转 90° 的文本（竖排） | `text` `w` `h` | 缺 `w`/`h` **报错** |
| `image` | 用户位图 | `image` | 图不可用只**告警**、不绘制 |
| `rect` | 纯色矩形 | `w` `h` | 缺 `w`/`h` **报错**；`border_width>0` 画空心边框 |
| `barcode` | code128 条形码（竖排、带文字） | `text` | `w`→50，`h`→180 |
| `corner_fade` | 右下角程序化渐变三角，**位置固定右下** | — | `w`（=目标半径）≤1 时取 192 |

`text_rot90` / `barcode` 是竖排渲染，`w` 是竖排后的窄边、`h` 是文字延伸方向，给 `h` 要留够文字长度。

---

## 4. 动画（`animation`）

`animation` 缺省 / 非法 / 与 `type` 不匹配 → 退回 `none`（解析器写告警日志）。

| animation | 适用 type | `speed` 语义 | 默认 speed | 永久循环 |
|---|---|---|---|---|
| `none` | 全部 | — | 0 | 否（`start_frame` 一次画出后保持） |
| `typewriter` | text | 每 codepoint 帧数 | 3 | 否 |
| `eink` | image / rect / barcode | 每闪烁态帧数 | 15 | 否（黑白闪几次后出内容） |
| `fade` | image / text / rect | 每帧不透明度增量 | 8 | 否（0→255 淡入） |
| `wipe` | rect / image | 划入总帧数（cubic-bezier 缓动） | 40 | 否（配 `direction`） |
| `scroll` | image | 每帧滚动像素 | 1 | **是**（垂直循环，除非设 `end_frame`） |
| `grow` | corner_fade | 每帧半径增量 | 10 | 否 |
| `move` | 除 corner_fade | 滑入总帧数 | 15 | 否（**必须**给 `from_dx`/`from_dy`，否则退 `none`） |
| `scramble` | text | 每 codepoint 帧数 | 2 | 否（乱码逐字稳定成真实文本） |
| `blink` | 除 corner_fade | 半周期帧数 | 15 | **是**（50% 占空，除非设 `end_frame`） |
| `sprite` | image | 每帧停留帧数 | 4 | **是**（横向精灵图逐帧，除非设 `end_frame`） |
| `sway` | 除 corner_fade | 一个来回周期帧数 | 60 | **是**（正弦晃动，除非设 `end_frame`） |

> `speed` 不填或 ≤0 时取该动画的默认值。

---

## 5. 字段速查

| 字段 | 类型 | 适用 | 说明 |
|---|---|---|---|
| `type` | string | 全部 | 见 §3，必填 |
| `animation` | string | 全部 | 见 §4，缺省 `none` |
| `x` `y` | int | 全部 | 360 基准。右/下锚定时是元素对应边到屏幕边的距离 |
| `w` `h` | int | 见 §3 | 360 基准。`corner_fade` 的 `w`=目标半径 |
| `anchor` | string | 全部 | `tl`(默认)/`tr`/`bl`/`br`，量起角落 |
| `start_frame` | int | 全部 | 动画起始帧，默认 0 |
| `end_frame` | int | 全部 | `>0`：到该帧隐藏并计完成；`0`=不退场。给循环元素设退场用 |
| `speed` | int | 全部 | 语义随 `animation` 变，见 §4 |
| `direction` | string | wipe | `ltr`(默认)/`rtl`/`ttb`/`btt` |
| `from_dx` `from_dy` | int | move / sway | move: 起点相对落点偏移；sway: 半摆幅（360 基准）。两者必填 |
| `frames` | int | image | sprite: 横向精灵图帧数（图宽须为其整数倍），≥2 |
| `text` | string | text/text_rot90/barcode | UTF-8，≤255 字节，`\n` 换行 |
| `font` | string | text/text_rot90 | `body`(CJK 正文)/`title`(重黑衬线)/`display`(Bebas 数字·编号)/`icon`(FontAwesome) |
| `font_size` | int | text/text_rot90 | 360 基准字号，默认 14 |
| `line_height` | int | text | 360 基准行高，0=字体默认 |
| `letter_space` | int | text/text_rot90 | 360 基准字距，默认 0 |
| `faux_bold` | bool | text_rot90 | 整体 +1px 重画加粗 |
| `bold_split` | bool | text_rot90 | 空格前加粗、空格后常规（无空格则整体加粗） |
| `color` | string | text/rect/corner_fade | `"#RRGGBB"`，恒不透明（见 §2） |
| `border_width` | int | rect | `>0` 画空心边框（360 基准线宽），0=实心 |
| `image` | string | image | 素材包内相对路径 |

---

## 6. 循环能力边界

引擎的「结束」模型有两种：

- **一次性素材**：所有元素播完（`done`）→ worker 退出，**画面定格保持**在最后一帧。
- **永久循环**：只要存在 `end_frame==0` 的 `scroll` / `blink` / `sprite` / `sway` 元素，引擎**永不自然结束**，一直跑到被 `overlay_abort` 打断。

**能永久循环的原语**：

| 原语 | 效果 | 备注 |
|---|---|---|
| `scroll` | 垂直循环滚动 | 只支持垂直 |
| `blink` | 周期闪烁 | 固定 50% 占空 |
| `sprite` | 横向精灵图逐帧播放 | 一张 strip，`frames` 均分，`speed`=每帧停留帧数 |
| `sway` | 沿 `from_dx/from_dy` 正弦晃动 | 简谐运动，端点慢中间快，自然浮动 |

`sway` 用来做「风中轻摆 / 悬浮物起伏」：`from_dx/from_dy` 是半摆幅方向与大小（水平摆给 `from_dx`，垂直摆给 `from_dy`，斜向两者都给），`speed` 是一个完整来回的帧数。示例（logo 上下轻浮，2 秒一个来回）：

```json
{ "type": "image", "image": "logo.png", "anchor": "br", "x": 10, "y": 10,
  "animation": "sway", "from_dy": 4, "speed": 60, "start_frame": 30 }
```

`sprite` 做逐帧动画（如旋转指示器）：把 N 帧横向拼成一张图，`frames` 填帧数（图宽须为 `frames` 整数倍）：

```json
{ "type": "image", "image": "spinner_strip.png", "x": 320, "y": 200,
  "animation": "sprite", "frames": 8, "speed": 3 }
```

**仍做不到**（需要扩引擎，见 §7）：
- 水平跑马灯（`scroll` 只垂直；`sway` 是往复不是单向滚动）
- 可调占空比的闪烁
- 呼吸式脉冲淡入淡出（`sway` 动的是位置不是 alpha）
- 整段序列定时重播

**已知边界**：永久循环下内部帧计数是 32 位有符号整数，连续运行约 **828 天**后溢出，届时 `blink` 相位会抖一帧；`scroll` 不受影响（用独立累加器）。设备重启即复位，实际无害，不做特殊处理。

---

## 7. 扩展：加一个新 animation

引擎是通用元素状态机，加新动画不用动分组/落盘框架，改四处即可（以加一个 `pulse` 呼吸动画为例）：

1. `src/overlay/opinfo.h` — `opinfo_anim_t` 加枚举项，注释适用 type。
2. `src/prts/operators.c`
   - `k_anim_map[]` 加一行：`{ "pulse", OPINFO_ANIM_PULSE, <默认speed> }`。
   - `anim_valid_for_type()` 加 case，声明适用哪些 type。
3. `src/overlay/opinfo.c`
   - `el_advance()` 加 case：每帧推进 `st` 里的运行态、置 `st->dirty`。
   - `el_draw()` 在对应 type 分支读该运行态绘制。
   - 若是永久循环动画，在 `overlay_opinfo_show_elements()` 里把它加进 `has_loop` 判定（或依赖 `end_frame` 退场）。
4. 若需要新的 per-element 运行态，在 `opinfo_el_state_t` 加字段（堆分配，worker 内回收）。

`sprite`（一次性播放视角外的永久循环）与 `sway`（正弦晃动、定点 `lv_trigo_sin` 无浮点）就是照此扩出来的，可作现成参考。新增循环原语记得同步更新本文 §4 与 §6。
