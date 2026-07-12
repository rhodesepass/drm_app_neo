#!/usr/bin/env python3
"""一次性: 把 drm_app_neo 当前依赖"动态扫描"得到的子集输入固化成静态 txt,
放到 buildroot board 下,供独立的 epass-fonts 包在 host 侧子集化时使用
(这样 buildroot 侧无需 drm_app_neo 源码 / 12M 的 character_table.json)。

产物:
  <board>/charset/common.txt     常用字+标点 (直接拷 common_char.txt)
  <board>/charset/operators.txt  方舟干员名用字 (从 character_table.json 提取, 可选)
  <board>/charset/literals.txt   drm_app_neo/src 里出现的中日文/全角字面量
  <board>/icons.txt              图标码点 (icons.h 私有区 + 全套 LV_SYMBOL), 每行一个十六进制
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
BOARD = os.path.normpath(os.path.join(
    REPO, "..", "buildroot", "board", "rhodesisland", "epass", "fonts"))
CHARSET = os.path.join(BOARD, "charset")

CJK_LITERAL_RE = re.compile(r"[⺀-⿟　-〿぀-ヿ㐀-䶿一-鿿豈-﫿＀-￯]")


def read_text(p):
    with open(p, "r", encoding="utf-8", errors="ignore") as f:
        return f.read()


def gen_operators():
    path = os.path.join(HERE, "character_table.json")
    if not os.path.isfile(path):
        print("  [跳过] 无 character_table.json")
        return set()
    data = json.loads(read_text(path))
    chars = set()
    for k, ch in data.get("Characters", {}).items():
        if not k.startswith("char_"):
            continue
        for field in ("Appellation", "Name"):
            for c in (ch.get(field) or ""):
                if c not in ("\n", "\r"):
                    chars.add(c)
    return chars


def gen_literals():
    chars = set()
    src = os.path.join(REPO, "src")
    for root, _, files in os.walk(src):
        for name in files:
            if not name.endswith((".c", ".h")):
                continue
            for ch in CJK_LITERAL_RE.findall(read_text(os.path.join(root, name))):
                chars.add(ch)
    return chars


def gen_icons():
    codes = set()
    icons_h = os.path.join(REPO, "src", "icons.h")
    if os.path.isfile(icons_h):
        txt = read_text(icons_h)
        for m in re.findall(r"\\u([0-9a-fA-F]{4})", txt):
            codes.add(int(m, 16))
        for m in re.findall(r"\\x([0-9a-fA-F]{2})", txt):
            codes.add(int(m, 16))
    sym = os.path.join(REPO, "lvgl", "src", "font", "lv_symbol_def.h")
    if os.path.isfile(sym):
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


def write_chars(path, chars):
    with open(path, "w", encoding="utf-8") as f:
        f.write("".join(sorted(chars)))
    print(f"  {os.path.relpath(path, BOARD):24s} {len(chars)} 字")


def main():
    os.makedirs(CHARSET, exist_ok=True)
    # common: 原样搬 common_char.txt (去换行)
    common = set(c for c in read_text(os.path.join(HERE, "common_char.txt"))
                 if c not in ("\n", "\r"))
    write_chars(os.path.join(CHARSET, "common.txt"), common)
    write_chars(os.path.join(CHARSET, "operators.txt"), gen_operators())
    write_chars(os.path.join(CHARSET, "literals.txt"), gen_literals())

    icons = gen_icons()
    with open(os.path.join(BOARD, "icons.txt"), "w", encoding="utf-8") as f:
        f.write("# FontAwesome / LV_SYMBOL 码点 (十六进制, 每行一个)\n")
        for c in sorted(icons):
            f.write(f"{c:04X}\n")
    print(f"  icons.txt                {len(icons)} 码点")
    print("完成 ->", BOARD)


if __name__ == "__main__":
    main()
