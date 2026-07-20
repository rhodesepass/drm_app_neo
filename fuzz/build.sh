#!/usr/bin/env bash
#
# 构建 drm_app_neo 的解析器 fuzz target。全部用 clang libFuzzer + ASan + UBSan,
# 纯 host 编译, 不需要交叉工具链/设备 —— 被测代码(src/vdec, src/prts 解析路径)
# 与硬件无关, 只吃字节。
#
# 用法:   fuzz/build.sh          # 编三个 target
# 依赖:   clang(带 compiler-rt fuzzer), pkg-config, libdrm 头(仅 epconfig 需要)
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"                     # drm_app_neo
cd "$ROOT"

CC="${CC:-clang}"
SAN="-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -g -O1"
mkdir -p "$HERE/findings"

echo "[*] fuzz_h264 (mp4样本外的裸码流: nalu 切分 + SPS/PPS/slice 解析)"
$CC $SAN -Isrc \
    "$HERE/fuzz_h264.c" \
    src/vdec/h264_parser.c src/vdec/nalu.c src/vdec/bitreader.c \
    -o "$HERE/fuzz_h264"

echo "[*] fuzz_mp4 (ISO-BMFF 解封装: box 遍历 + stsz/stco/stsc 采样表)"
$CC $SAN -Isrc \
    "$HERE/fuzz_mp4_demux.c" \
    src/vdec/mp4_demux.c \
    -o "$HERE/fuzz_mp4"

echo "[*] fuzz_epconfig (epconfig.json 字段校验: prts_operator_try_load)"
$CC $SAN -Ifuzz/stubs -Isrc $(pkg-config --cflags libdrm) \
    "$HERE/fuzz_epconfig.c" \
    src/prts/operators.c src/utils/misc.c src/utils/cJSON.c src/utils/uuid.c \
    -o "$HERE/fuzz_epconfig"

echo "[+] done. 见 fuzz/run.sh"
