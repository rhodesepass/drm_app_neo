#!/bin/bash
# 递归收集 Windows exe 依赖的 mingw DLL 到 exe 同级（或指定目录）。
# 只拷贝 sysroot(/mingw64/bin) 里有的 DLL —— 不在 sysroot 的一律视为 Windows
# 系统 DLL（kernel32/user32…），跳过。这样产物目录双击即可运行。
#
# 用法: collect-dll.sh <path/to/app.exe> [output_dir]
set -euo pipefail

EXE="${1:?usage: collect-dll.sh <exe> [outdir]}"
OUTDIR="${2:-$(dirname "$EXE")}"
SYSROOT_BIN="${MINGW_SYSROOT:-/mingw64}/bin"
OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"

mkdir -p "$OUTDIR"
declare -A seen

copy_deps() {
  local f="$1"
  local d src
  for d in $("$OBJDUMP" -p "$f" 2>/dev/null | awk '/DLL Name:/ {print $3}'); do
    [[ -n "${seen[$d]:-}" ]] && continue
    seen[$d]=1
    src="$SYSROOT_BIN/$d"
    if [[ -f "$src" ]]; then
      cp -u "$src" "$OUTDIR/"
      copy_deps "$src"   # 递归解析这个 DLL 自己的依赖
    fi
  done
}

copy_deps "$EXE"
echo "DLLs collected into: $OUTDIR"
ls -1 "$OUTDIR"/*.dll 2>/dev/null | wc -l | xargs echo "  count:"
