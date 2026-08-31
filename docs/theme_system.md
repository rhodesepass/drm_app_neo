# 主题系统 开发指南

> 面向**手写 UI 的开发者**：怎么在屏幕里挂样式类、怎么新增样式类、怎么新增一套主题。
> 本文是主题系统的完整开发指南；只借用「class」的思路，不用字符串做键。

## 基本约束

主题分两层，职责分开：

| 层 | 类型 | 管什么 | 定义位置 |
|---|---|---|---|
| 颜色层 | `ui_color_role_t`（`UI_C_*`） | 语义颜色：primary / warning / danger / … | 各主题文件里的 preset `pal[]`（如 `theme_old.c`） |
| 样式类层 | `ui_class_t`（`UI_CLS_*`） | 整套外观：底色 / 圆角 / 阴影 / 边框 / 选中效果 / 子节点 | `src/ui/ui_theme/theme_old.c`（类表） |

核心约束：**类表里的颜色字段填的是颜色角色（不是 hex）**，因此切主题时颜色和外观一起翻转；
**coord 字段一律 360 基准**，由 `apply_class_state` / `add_class` 统一套 `S()`。

### 1) 数据流与切换时机

- 颜色层：`ui_color(role)` 查当前 preset 的 `pal[]`，枚举下标 O(1)。
- 样式类层：`ui_theme_class(cls)` 返回当前主题下某类的 `const ui_class_def_t *`。
- 切主题链路：`ui_theme_apply(id)` → `styles_apply_palette()`（重着色共享 style + 整表重设类）→ `lv_obj_report_style_change()`。
- 类表是 `static const`，编译进 flash，无动态分配；只在 apply 那一刻重设，**每帧渲染零开销**。

### 2) 在屏幕里挂类

```c
lv_obj_t *b = ui_text_button(root, 9, 137, 160, 51, UI_SEM_DEFAULT, "文件/MTP", on_mtp);
add_class(b, UI_CLS_BTN_SELECT);
```

（`ui_text_button` / `ui_small_text_button` / `ui_place` 内部都套 `S()`，屏内传 360 基准裸数字，**不要**再包 `S()`。）

- UI 文件只声明「这里有个按钮 + 默认内容（文字 / 图标）+ 默认尺寸」。
- 按钮用 `UI_SEM_DEFAULT`（不预置语义底色），底色完全交给类，避免和类表里的 `bg` 打架。

`add_class` 内部做的事：

1. 挂共享样式：`s_cls_def[cls]`（`LV_STATE_DEFAULT`）+ `s_cls_foc[cls]`（`LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY`）+ `s_cls_edit[cls]`（`LV_STATE_USER_1`）。
2. `focus_anim`：类表开了聚焦动画则挂 150ms 过渡。
3. `no_focus_ring`：压掉默认焦点外框（改用主题自己的选中指示器）。
4. `label_font_*`：覆盖首个子 label 字体（如 confirm 的图标字形）。
5. `w` / `h`：以局部样式覆盖按钮尺寸（0 = 不覆盖）。
6. `build_content` 钩子：主题添加 / 覆盖内容子节点（如 `>` 指示器）。

### 3) 类描述符字段

#### 3.1 `ui_class_state_t` —— 一个状态（默认 / 聚焦）的外观

颜色字段填 `ui_color_role_t`，`UI_C_COUNT` = 不设 / 继承。coord 字段 360 基准。

| 字段 | 含义 |
|---|---|
| `bg` / `text` / `border` / `shadow` | 底色 / 文字 / 边框 / 阴影的颜色角色（`UI_C_COUNT` = 继承） |
| `bg_opa` | 底色不透明度 |
| `radius` | 圆角（`-1` = 不设，用 LVGL 默认主题圆角；切主题时自动清旧值） |
| `pad_all` | 内边距 |
| `shadow_width` / `shadow_ofs_x` / `shadow_ofs_y` / `shadow_opa` | 阴影宽 / 偏移 / 不透明度 |
| `border_width` / `border_side` / `border_opa` / `border_post` | 边框宽 / 侧 / 不透明度 / 是否画在子对象之上 |

#### 3.2 `ui_class_def_t` —— 一个类的完整定义

| 字段 | 含义 |
|---|---|
| `name` | 类名（仅调试 / 日志用；查找键是 `ui_class_t` 枚举，不是这个字符串） |
| `def` / `foc` / `edit` | 默认态 / 聚焦态 / 编辑(排序)态外观（`edit` 挂 `LV_STATE_USER_1`，如 oplist 排序拎起高亮） |
| `no_focus_ring` | `true` = 压掉默认焦点外框 |
| `label_font_role` / `label_font_px` | 覆盖子 label 字体（`px > 0` 生效） |
| `label_text` | 覆盖首个子 label 文本（如 confirm 的 `确定/取消` 或图标码点，`NULL` = 不改） |
| `w` / `h` | 覆盖按钮宽高（360 基准，0 = 不覆盖，沿用 UI 文件 `lv_obj_set_size`） |
| `grid_layout` | 主菜单宫格内容布局（仅 `UI_CLS_MAIN_GRID`，NULL = 用默认对齐） |
| `build_content` | 内容子节点钩子，`NULL` = 不加额外子节点 |

### 4) 新增样式类怎么做

1. `src/ui/ui_theme.h` 的 `ui_class_t` 枚举里，在 `UI_CLS_COUNT` 前加成员 `UI_CLS_XXX`。
2. 在**默认表 + 每个 advanced 主题的类表**里各加 `[UI_CLS_XXX] = {...}`：
   - `src/ui/ui_theme/theme_default.c` 的 `ui_theme_classes_default`（内置回退）。
   - 各高级主题文件里的类表（如 `theme_old.c` 的 `ui_theme_classes_old`）。
3. 屏幕里 `add_class(obj, UI_CLS_XXX)`。

> ⚠️ 改了枚举顺序（增 / 删 `UI_CLS_*` 或 `UI_C_*` 成员）要 clean rebuild：旧 `.o` 里枚举索引错位会出现「谜之颜色」。

### 5) 新增一套主题怎么做（从零到能跑）

**最快路径**：把 [theme_sample.c](theme_sample.c) 复制到 `src/ui/ui_theme/theme_xxx.c`（并建同名 `.h`），改预设里的名字 / 色板 / 类表即可——它就是一套完整可注册的模板。

手动四步：

1. 建 `src/ui/ui_theme/theme_xxx.h`：

   ```c
   #pragma once
   #include "ui/ui_theme.h"
   extern const ui_class_def_t  ui_theme_classes_xxx[];
   extern const ui_theme_preset_t ui_theme_preset_xxx;
   ```

2. 建 `src/ui/ui_theme/theme_xxx.c`：定义类表 `ui_theme_classes_xxx[]` + preset `ui_theme_preset_xxx`（字段见下）。
3. 在 `src/ui/ui_theme/theme.h` 里加一行 `#include "ui/ui_theme/theme_xxx.h"`（索引登记）。
4. 在 `src/ui/ui_theme/themes.c` 的注册表里加一行：

   ```c
   &ui_theme_preset_xxx,
   ```

源文件由 CMake 的 `file(GLOB ... CONFIGURE_DEPENDS src/ui/ui_theme/*.c)` 自动收录，无需改 `CMakeLists.txt`；改完重新编译即可。

**硬性约束（漏了会出谜之外观）：**

- 类表**必须覆盖全部 `UI_CLS_*`**（缺哪个类，那个元素的样式就是全零/继承，外观会崩）。参考 `theme_sample.c` 的 10 个类。
- 色板 `pal[]` **必须覆盖全部 `UI_C_*` 角色**（缺的角色 `ui_color()` 返回黑 0x000000）。`theme_sample.c` 有完整角色示例。
- 类表颜色字段填**角色**（不是 hex）；coord 一律 360 基准。

**preset 字段一览（`ui_theme_preset_t`）：**

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` | ✅ | 主题显示名（设置屏下拉） |
| `dark` | ✅ | LVGL 基础主题深浅，开关/下拉/滚动条等标准控件随它 |
| `advanced` | ✅ | `true` = 用本文件类表；`false` = 用内置 `ui_theme_classes_default` 回退表 |
| `pal[UI_C_COUNT]` | ✅ | 完整色表（覆盖全部角色） |
| `cls` | advanced 时 ✅ | 类表指针 |
| `menu_icons` | 可选 | 主菜单 6 入口图标码点表，NULL = 不提供 |
| `ofs` | 可选 | 坐标偏移表（`[UI_OF_SLOT_COUNT]`，NULL = 全基准 0） |
| `decorate` | 可选 | 页面装饰钩子（顶栏/风格条/底部条），NULL = 默认页头 `ui_header` |
| `header` | 可选 | 页头钩子（logo+标题），NULL = `ui_header` |
| `gauge` | 可选 | 存储仪表钩子（arc/bar），NULL = 默认 arc；`gauge = ui_theme_gauge_bar` 换 bar |
| `titles` | 可选 | 各屏标题覆盖表（下标 `screen_id_t`），NULL = 屏内默认 |
| `size_label_dx/dy` | 可选 | displayimg 图片大小 label 偏移 |

**验证 / 预览：**

- PC 预览：`\dist\windows\app_pc_360.exe preview --theme <名|id> [页名] [间隔ms]`（`--theme` 大小写不敏感、也接受数字下标）。
- 或真机/PC 直接进 设置屏 → 主题下拉 切到新主题（`ui_theme_apply` 会自动重建屏）。

### 6) 设计考量

#### 6.1 为什么颜色填角色而不是 hex？

颜色角色（`UI_C_*`）让一套类表能被多套配色方案复用：切主题只换 `pal[]`，类表里的几何 / 阴影 / 选中效果不用动。填 hex 会把外观和具体主题焊死。

#### 6.2 为什么查找键是枚举而不是字符串 name？
枚举键编译期查错、O(1) 下标、零运行时开销；字符串键要 `strcmp` 线性查找，拼错要运行时才暴露，在本项目弱端（F1C200s / T113）不划算。`ui_class_def_t.name` 只作调试 / 日志标签。

#### 6.3 为什么尺寸覆盖走局部样式而不是共享样式？

`ui_text_button` 用 `lv_obj_set_size()` 设尺寸，写的是**局部样式**（LVGL 里优先级最高，压过共享样式）。因此 `add_class` 里用 `lv_obj_set_width/height`（同样是局部样式，且在 `lv_obj_set_size` 之后调用）才能覆盖；共享样式里的 width / height 压不过它。

## 屏幕坐标偏移与存储仪表

### 坐标放置：ui_place

屏内一律以**旧版 (main 分支) 坐标**写基准；主题用 preset 的 `ofs[]` 表把元素平移到自己的布局。这样 Old 主题（`ofs=NULL`）天然就是旧观感；新主题通过 ofs 表偏移到自己的布局。

```c
// screen_common.h —— 放置元素：基准坐标 + 主题偏移/尺寸覆盖
void ui_place(lv_obj_t *obj, int base_x, int base_y, ui_of_slot_t slot);
```

`ui_place` 做的事：

1. 取当前主题该槽位的偏移 `ui_theme_ofs(slot)`（无则全零）。
2. `lv_obj_set_pos(obj, S(base_x + dx), S(base_y + dy))` —— **内部套 `S()`**，`base_x/y`、`dx/dy` 都按 360 基准书写。
3. 若 `w/h > 0`，再以 `S(w)/S(h)` 覆盖尺寸（0 = 不覆盖，沿用屏内 `lv_obj_set_size`）。

**槽位**：`ui_of_slot_t`（`ui_theme.h`，按页面分组）给每个可被主题平移的元素一个槽位；未在主题 `ofs[]` 里列出的槽位 = 基准坐标 (0)，无需偏移。

**示例**（`screen_sysinfo.c`）：

```c
lv_obj_t *t1 = lv_label_create(root);
ui_place(t1, 52, 180, UI_OF_SLOT_SYS_LBL_NAND);   // 旧版基准 (52,180)，主题可整体平移
```

**按钮约定**：`ui_text_button` / `ui_small_text_button` / `ui_place` **内部都套 `S()`**，屏内一律传 360 基准裸数字，**不要再用 `S()` 包裹**（曾因此 720 双倍缩放）。按钮的尺寸/文字另走类表（`w/h`、`label_text`）；纯坐标平移走槽位（可逐实例不同，如 usbselect 左/右列偏移不同）。

### 存储仪表 (sysinfo)

`ui_theme_preset_t.gauge` 是主题钩子：决定 sysinfo 用 arc 还是 bar，并自行定位（可叠加偏移）。未提供时回退默认 arc（`ui_theme.c`）；preset 直接 `gauge = ui_theme_gauge_bar` 可用内置 bar。两种内置仪表共用 `UI_C_GAUGE_TRACK/INDICATOR` 主题色（`add_style_gauge`）。屏内 `ui_theme_gauge(root, slot, x, y, w, h, &pct)` 构建；tick 用 `lv_obj_check_type` 按 arc/bar 分派 `lv_*_set_value`。

### 页面标题覆盖

各屏页头标题默认用屏内写死的文案；preset 提供 `titles[]`（下标 = `screen_id_t`，项为 NULL 用默认）可整体覆盖，正常情况下不动。屏内统一用 `ui_theme_header(root, ui_theme_title(SCREEN_X, "默认标题"))` 保留覆盖接口。

## 开发示例

- [theme_sample.c](theme_sample.c) 是一套主题的**从零参考模板**（类表 + preset + 完整色板），复制到 `src/ui/ui_theme/` 并按文件头注释注册即可启用。
- [theme_old.c](../src/ui/ui_theme/theme_old.c) 是仓库内实际注册的旧观感主题（`ofs=NULL` 即旧布局）。
- [theme_default.c](../src/ui/ui_theme/theme_default.c) 的内置 `ui_theme_classes_default` 是 `advanced=false` 主题的回退基线。
- [themes.c](../src/ui/ui_theme/themes.c) 是 preset 注册表。
