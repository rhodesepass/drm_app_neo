#!/usr/bin/env python3
"""
字体子集化独立工具 —— 把 font/original/ 的全量字体裁成 app 实际用到的字形，写到 font/。

思源黑/宋是 16M/24M 的大二进制，全量入 git 不现实。这个工具按项目真正会显示的字符
把它们裁到几 M，产物用原文件名放到 font/ 根，随仓库提交；原件留在 font/original/
(gitignore) 本地备用。

三类字体各自的保留策略：
  - 思源黑 (BODY) / 思源宋 (TITLE): common_char.txt(3500通用字) + 方舟干员名(character_
    table.json，可选) + 源码里出现的所有中日文字面量 + 假名/中文标点整段。
  - FontAwesome Solid (ICON): 只留 src/icons.h 用到的私有区码点，外加全套 LV_SYMBOL
    (LVGL 内建控件如 dropdown 箭头会经 fallback 落到这个字体)。
  - BebasNeue (DISPLAY): 拉丁小字体，本就 63K，原样拷贝。

裁剪后的字体丢弃了 GSUB/GPOS —— 我们只做横排定字号位图渲染 (FreeType)，用不到连字/
字距调整，这一刀是思源体积的大头。

用法(仓库内直接跑，无参数即可):
    python3 tools/font_generate/subset_fonts.py
可选:
    --original DIR   原件目录 (默认 ../../font/original 相对本脚本)
    --out DIR        输出目录 (默认 ../../font)
    --src DIR        扫源码字面量的根 (默认 ../../src)
"""

import argparse
import os
import re
import shutil
import sys

try:
    from fontTools import subset
    from fontTools.ttLib import TTFont
except ImportError:
    sys.exit("需要 fonttools: pip install fonttools")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

# 保留整段的码点区间 (start, end 含端点)。思源都有这些字形，成本几百个字形可忽略，
# 换取干员名里的假名和各种全角标点不缺字。
KANA_PUNCT_RANGES = [
    (0x0020, 0x007E),  # ASCII 可打印
    (0x00A0, 0x00FF),  # Latin-1 补充 (°×÷ 等)
    (0x2000, 0x206F),  # 常用标点 (— … “ ” ‘ ’ 等)
    (0x2070, 0x209F),  # 上标下标
    (0x20A0, 0x20CF),  # 货币符号 (￥ € 等)
    (0x2100, 0x214F),  # 字母式符号 (№ ™ 等)
    (0x2460, 0x24FF),  # 带圈数字/字母 (① 等)
    (0x2500, 0x257F),  # 制表符 (文件树可能用)
    (0x25A0, 0x25FF),  # 几何图形 (■ ● ▲ 等)
    (0x3000, 0x303F),  # CJK 符号与标点 (、。《》 等)
    (0x3040, 0x309F),  # 平假名
    (0x30A0, 0x30FF),  # 片假名
    (0xFF00, 0xFFEF),  # 全角/半角形式
]


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return f.read()


def collect_common_chars(original_dir):
    """common_char.txt: 3500 通用字 + 拉丁数字 + 中文标点。"""
    path = os.path.join(HERE, "common_char.txt")
    chars = set()
    for ch in read_text(path):
        if ch not in ("\n", "\r"):
            chars.add(ord(ch))
    return chars


def collect_operator_names():
    """方舟 character_table.json 里的干员名 (Appellation 拉丁名 + Name 中文名)。
    文件靠特殊手段获取、本地 gitignore；不存在就跳过，仅少覆盖生僻名用字。"""
    import json
    path = os.path.join(HERE, "character_table.json")
    if not os.path.isfile(path):
        print("  [跳过] 无 character_table.json，干员名用字不纳入 (可能有生僻字缺字)")
        return set()
    data = json.loads(read_text(path))
    chars = set()
    n = 0
    for k, ch in data.get("Characters", {}).items():
        if not k.startswith("char_"):
            continue
        for field in ("Appellation", "Name"):
            for c in (ch.get(field) or ""):
                chars.add(ord(c))
        n += 1
    print(f"  干员条目 {n}，纳入 {len(chars)} 个用字")
    return chars


CJK_LITERAL_RE = re.compile(
    r"[⺀-⿟　-〿぀-ヿ㐀-䶿一-鿿豈-﫿＀-￯]"
)


def collect_source_literals(src_dir):
    """扫 .c/.h 里出现的中日文/全角字面量 —— UI 静态文案必须能渲染，通用字表大概率
    已覆盖，这一步是零成本的保险。"""
    chars = set()
    for root, _, files in os.walk(src_dir):
        for name in files:
            if not name.endswith((".c", ".h")):
                continue
            for ch in CJK_LITERAL_RE.findall(read_text(os.path.join(root, name))):
                chars.add(ord(ch))
    return chars


def collect_icon_codepoints(src_dir):
    """FontAwesome 需要的码点：src/icons.h 里的 \\uXXXX / \\xXX，加全套 LV_SYMBOL。
    LVGL 内建控件通过文字字体的 fallback 落到这个图标字体，所以 LV_SYMBOL 全带上。"""
    codes = set()

    icons_h = os.path.join(src_dir, "icons.h")
    if os.path.isfile(icons_h):
        txt = read_text(icons_h)
        for m in re.findall(r"\\u([0-9a-fA-F]{4})", txt):
            codes.add(int(m, 16))
        for m in re.findall(r"\\x([0-9a-fA-F]{2})", txt):
            codes.add(int(m, 16))

    sym = os.path.join(REPO, "lvgl", "src", "font", "lv_symbol_def.h")
    if os.path.isfile(sym):
        # 每个 LV_SYMBOL 是一串 \xHH UTF-8 字节，拼回来解码成码点
        for line in read_text(sym).splitlines():
            m = re.search(r'#define\s+LV_SYMBOL_\w+\s+"((?:\\x[0-9a-fA-F]{2})+)"', line)
            if not m:
                continue
            raw = bytes(int(b, 16) for b in re.findall(r"\\x([0-9a-fA-F]{2})", m.group(1)))
            try:
                codes.add(ord(raw.decode("utf-8")))
            except (UnicodeDecodeError, TypeError):
                pass
    return codes


def do_subset(src, dst, unicodes, drop_layout=True):
    opts = subset.Options()
    opts.glyph_names = False          # 丢字形名，省体积
    opts.name_IDs = ["*"]             # 保留 name 表 (版权/字体名，很小)
    opts.name_legacy = True
    opts.name_languages = ["*"]
    opts.recalc_timestamp = False
    opts.notdef_outline = True        # 缺字时有个可见的 .notdef 而非崩
    opts.desubroutinize = True        # CFF 裁完去子程序调用，利于压缩
    opts.hinting = True               # 小字号要靠 hint，别丢
    if drop_layout:
        opts.layout_features = []     # 丢 GSUB/GPOS —— 横排位图用不到，思源省体积大头

    font = TTFont(src)
    subsetter = subset.Subsetter(options=opts)
    subsetter.populate(unicodes=sorted(unicodes))
    subsetter.subset(font)
    font.save(dst)
    font.close()


def fmt_size(path):
    return f"{os.path.getsize(path) / 1024 / 1024:.2f}M"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--original", default=os.path.join(REPO, "font", "original"))
    ap.add_argument("--out", default=os.path.join(REPO, "font"))
    ap.add_argument("--src", default=os.path.join(REPO, "src"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    print("[1/3] 构建中日文字符集")
    cjk = set()
    for lo, hi in KANA_PUNCT_RANGES:
        cjk.update(range(lo, hi + 1))
    cjk |= collect_common_chars(args.original)
    cjk |= collect_operator_names()
    cjk |= collect_source_literals(args.src)
    print(f"  中日文字符集共 {len(cjk)} 码点")

    print("[2/3] 收集图标码点")
    icons = collect_icon_codepoints(args.src)
    print(f"  图标共 {len(icons)} 码点")

    # (原文件名, 策略)。产物沿用原名，app/font_registry 无需改动。
    jobs = [
        ("SourceHanSansSC-Regular.otf", "cjk"),
        ("SourceHanSerifSC-Heavy.otf",  "cjk"),
        ("Font-Awesome-7-Free-Solid-900.otf", "icon"),
        ("BebasNeue.otf", "copy"),
    ]

    print("[3/3] 裁剪")
    for name, kind in jobs:
        src = os.path.join(args.original, name)
        dst = os.path.join(args.out, name)
        if not os.path.isfile(src):
            print(f"  [缺] {name} 不在 {args.original}，跳过")
            continue
        before = fmt_size(src)
        if kind == "copy":
            shutil.copyfile(src, dst)
        elif kind == "icon":
            do_subset(src, dst, icons, drop_layout=True)
        else:
            do_subset(src, dst, cjk, drop_layout=True)
        print(f"  {name:38s} {before:>8s} -> {fmt_size(dst):>8s}")

    print("完成。产物在", args.out)


if __name__ == "__main__":
    main()
