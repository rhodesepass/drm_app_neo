#!/usr/bin/env bash
#
# Monkey test: PC app 用 ASan+UBSan 编译, 无头(SDL dummy)长跑, 每 40ms 用确定性
# PRNG 注入一个随机导航键。断言只有一条: 别崩、别 UB。抓的是人测不到的角落 ——
# 快速连按、边界屏来回切、动画未完就切走。
#
# 前置: 先建 sanitizer 版 app(见下方 BUILD 注释), 且 build-asan/pcdata 里有素材。
#
# 用法:
#   fuzz/monkey.sh <每个种子秒数> <种子个数>     默认 60 4
#   崩溃复现:  EPASS_MONKEY="<seed>:40:<ms>" 重跑同一 seed 即 100% 复现
#
# BUILD (一次性):
#   SANF="-fsanitize=address,undefined -fno-sanitize=function -fno-sanitize-recover=undefined -g"
#   cmake -S . -B build-asan -DTARGET_PC=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
#         -DCMAKE_C_FLAGS="$SANF" -DCMAKE_EXE_LINKER_FLAGS="$SANF"
#   cmake --build build-asan --target app_pc_360 -j$(nproc)
#   cp -r build-pc/pcdata/* build-asan/pcdata/    # 需要真实素材
# 注: 关掉 -fsanitize=function —— 它会误报 LVGL 回调惯用的函数指针类型转换, 全是噪音。
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
APP="$ROOT/build-asan/app_pc_360"
SECS="${1:-60}"
NSEED="${2:-4}"
[ -x "$APP" ] || { echo "先建 sanitizer 版 app (见本脚本 BUILD 注释)"; exit 1; }

cd "$ROOT/build-asan"
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:exitcode=99
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy

fail=0
for seed in $(seq 1 "$NSEED"); do
    log="$HERE/findings/monkey_seed${seed}.log"
    echo "[*] seed=$seed, ${SECS}s ..."
    EPASS_MONKEY="${seed}:40:$((SECS*1000))" \
        timeout $((SECS+20)) "$APP" > "$log" 2>&1 || true
    if grep -qa "runtime error\|AddressSanitizer\|heap-\|stack-buffer\|SUMMARY: .*Sanitizer" "$log"; then
        echo "    !! seed=$seed 触发 sanitizer, 见 $log"
        grep -a "runtime error\|AddressSanitizer\|SUMMARY:" "$log" | head -3
        fail=1
    else
        echo "    干净"
    fi
done
[ "$fail" = 0 ] && echo "[+] 全部干净" || { echo "[!] 有 seed 触发, 用 EPASS_MONKEY=<seed>:40:<ms> 复现"; exit 1; }
