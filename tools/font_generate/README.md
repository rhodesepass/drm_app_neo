# 字体子集化工具

> 迁移说明：设备侧字体已改为**系统共享方案**——由独立仓 `epass-fonts` + buildroot
> 包 `epass-fonts` 在 host 侧子集化并装到 `/usr/share/fonts/epass/`，app 构建期经
> pkg-config(`epass-fonts`) 取目录。本目录下的工具现在只服务两件事：
>   1. `export_board_charset.py`：扫本仓源码，把字表/图标码点导出到 buildroot
>      `board/rhodesisland/epass/fonts/`，供 `epass-fonts` 包子集化使用（换字体/加文案后重跑）。
>   2. `subset_fonts.py` + `font/`：仅剩模拟器 dev 回退用（顶层 CMake 用共享字体后设备侧不再依赖）。

思源黑/宋全量各 16M/24M，全量入 git 不现实。这个独立工具把它们裁成 app 实际会显示的
字形（几 M），产物用原文件名放到仓库 `font/` 根、随仓库提交；CMake 构建时从 `font/`
拷进 `res/fonts`。

## 目录约定

```
font/
├─ original/          # 全量原件，工具输入。.gitignore 掉，本地存放
│  ├─ SourceHanSansSC-Regular.otf
│  ├─ SourceHanSerifSC-Heavy.otf
│  ├─ Font-Awesome-7-Free-Solid-900.otf
│  └─ BebasNeue.otf
├─ SourceHanSansSC-Regular.otf      # ↓ 裁剪产物，入 git
├─ SourceHanSerifSC-Heavy.otf
├─ Font-Awesome-7-Free-Solid-900.otf
└─ BebasNeue.otf
```

## 用法

```sh
pip install fonttools          # 一次
python3 tools/font_generate/subset_fonts.py
```

无参数即按上面的默认路径跑。改路径见 `--original / --out / --src`。

换了字体、加了新的 UI 文案或图标后重跑一次，再提交 `font/` 下的产物。

## 保留策略（subset_fonts.py）

- **思源黑 (BODY) / 思源宋 (TITLE)**：`common_char.txt`（3500 通用字）+ 方舟干员名
  （`character_table.json`，可选）+ 源码里出现的所有中日文字面量 + 假名/中文标点整段。
  丢弃 GSUB/GPOS —— 只做横排定字号位图渲染，用不到连字/字距，这一刀是体积大头。
- **FontAwesome Solid (ICON)**：只留 `src/icons.h` 用到的私有区码点，外加全套
  `LV_SYMBOL`（LVGL 内建控件经文字字体 fallback 落到这里）。
- **BebasNeue (DISPLAY)**：拉丁小字体，原样拷贝。

干员名要覆盖生僻字，需自备 `character_table.json`（方舟角色表）放本目录，已 gitignore。
缺这个文件也能跑，只是干员名可能个别生僻字变豆腐块。

## 相关文件

- `subset_fonts.py`      —— 主工具：建字符集 + 裁字体。
- `generate_charlist.py` —— 旧脚本，只导出字符列表 `result.txt`（逻辑已并入 subset_fonts，
  想单独看字符集时用）。
- `common_char.txt`      —— 3500 通用字 + 拉丁数字 + 中文标点。
