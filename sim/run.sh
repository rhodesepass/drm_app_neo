#!/usr/bin/env bash
#
# 同时启动 360x640 与 720x1280 两档模拟器并排摆放，便于对比同一改动在双目标下的效果。
#
# 用法:
#   sim/run.sh [参数...]
#
# 参数可以是:
#   - 纯数字           → 作为 SIM_SCREEN (起始屏 id, 见 screen_id_t: 0=mainmenu ... 9=confirm)
#   - NAME=VALUE       → 导出为环境变量传给两个程序 (如 SIM_SCREEN=2、LOG_LEVEL=debug)
#
# 例:
#   sim/run.sh                 # 两档都从首屏(mainmenu)起
#   sim/run.sh 3               # 两档都直接跳到 spinner
#   sim/run.sh SIM_SCREEN=1    # 同上写法, 跳到 oplist
#
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$DIR/build"
BIN_360="$BUILD/epass_ui_sim_360x640"
BIN_720="$BUILD/epass_ui_sim_720x1280"

# --- 参数 → 环境变量 ---
for arg in "$@"; do
    case "$arg" in
        *=*)            export "$arg" ;;            # NAME=VALUE
        ''|*[!0-9]*)    echo "忽略无法识别的参数: $arg" >&2 ;;
        *)              export SIM_SCREEN="$arg" ;;  # 纯数字
    esac
done

# --- 每次启动前都构建 (首次或缺 cache 时先配置) ---
if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    cmake -S "$DIR" -B "$BUILD" >/dev/null
fi
echo "==> 构建两档..."
cmake --build "$BUILD" -j"$(nproc)"

# --- 并排摆放 ---  360 档贴左(基准 X), 720 档在其右侧 +400。基准可用 SIM_WIN_X/Y 调。
BASE_X="${SIM_WIN_X:-0}"
BASE_Y="${SIM_WIN_Y:-0}"
( SIM_WIN_X="$BASE_X"            SIM_WIN_Y="$BASE_Y" "$BIN_360" ) &
P1=$!
( SIM_WIN_X="$((BASE_X + 400))"  SIM_WIN_Y="$BASE_Y" "$BIN_720" ) &
P2=$!

echo "==> 360x640 pid=$P1   720x1280 pid=$P2   (SIM_SCREEN=${SIM_SCREEN:-<default>})"

# 任一退出 / Ctrl-C → 一起收尾
trap 'kill "$P1" "$P2" 2>/dev/null || true' INT TERM EXIT
wait -n "$P1" "$P2" 2>/dev/null || true
