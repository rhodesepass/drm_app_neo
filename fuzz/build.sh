#!/usr/bin/env bash
#
# 构建 drm_app_neo 的解析器 fuzz target。全部用 clang libFuzzer + ASan + UBSan,
# 纯 host 编译, 不需要交叉工具链/设备 —— 被测代码只吃字节。
#
# 解码栈(mp4/h264/h265 解析)已抽成独立包 srgnvdec, 它的 fuzz 全家桶跟包走:
# 见 ../srgnvdec/fuzz/。这里只剩 app 自己的解析面。
#
# 用法:   fuzz/build.sh
# 依赖:   clang(带 compiler-rt fuzzer), pkg-config, libdrm 头(epconfig 需要)
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"                     # drm_app_neo
cd "$ROOT"

CC="${CC:-clang}"
SAN="-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -g -O1"
mkdir -p "$HERE/findings"

echo "[*] fuzz_epconfig (epconfig.json 字段校验: prts_operator_try_load)"
$CC $SAN -Ifuzz/stubs -Isrc $(pkg-config --cflags libdrm) \
    "$HERE/fuzz_epconfig.c" \
    src/prts/operators.c src/utils/misc.c src/utils/cJSON.c src/utils/uuid.c \
    -o "$HERE/fuzz_epconfig"

echo "[+] done. 见 fuzz/run.sh"
