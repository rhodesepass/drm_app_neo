#!/usr/bin/env bash
# ============================================================================
# EPass DRM —— 一键交叉编译 Windows x86_64 .exe
#
# 全流程在 Docker 里跑（archlinux + mingw-w64 + MSYS2 预编译依赖），宿主只需
# 装 docker。build-win/ 里是一堆中间编译产物；脚本最后把运行时文件收进单个
# dist/windows/：360/720 两个 exe 同放一处，DLL/res/pcdata 只存一份（三者
# 与分辨率无关）。整目录拷到 Windows，按分辨率双击对应 exe 即可。
#
# 用法:
#   ./build_cross_windows.sh                 # 默认 360+720 都编（同一 dist）
#   ./build_cross_windows.sh 720             # 只编 app_pc_720
#   ./build_cross_windows.sh 360             # 只编 app_pc_360
#   ./build_cross_windows.sh --rebuild-image # 强制重建 docker 镜像
#   ./build_cross_windows.sh --clean         # 先清掉 build-win/ 再编
#   ./build_cross_windows.sh --release        # 传 -DAPP_RELEASE=ON
#
# 环境变量:
#   IMAGE       docker 镜像名（默认 epass-mingw）
#   BUILD_DIR   构建目录（默认 build-win）
#   DIST_DIR    打包输出目录（默认 dist/windows）
#   JOBS        并行度（默认 nproc）
# ============================================================================
set -euo pipefail

# 切到仓库根（脚本所在目录），保证相对路径一致
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

IMAGE="${IMAGE:-epass-mingw}"
BUILD_DIR="${BUILD_DIR:-build-win}"
DIST_DIR="${DIST_DIR:-dist/windows}"
DOCKERFILE="docker/Dockerfile.mingw"
TOOLCHAIN="cmake/toolchain-mingw64.cmake"

# ---- 解析参数 --------------------------------------------------------------
TARGETS=()
REBUILD_IMAGE=0
CLEAN=0
CMAKE_EXTRA=()

for arg in "$@"; do
  case "$arg" in
    720)  TARGETS+=("app_pc_720") ;;
    360)  TARGETS+=("app_pc_360") ;;
    all)  TARGETS+=("app_pc_720" "app_pc_360") ;;
    --rebuild-image) REBUILD_IMAGE=1 ;;
    --clean)         CLEAN=1 ;;
    --release)       CMAKE_EXTRA+=("-DAPP_RELEASE=ON") ;;
    -h|--help)
      sed -n '2,23p' "$0"; exit 0 ;;
    *)
      echo "未知参数: $arg（可用: 720 / 360 / --rebuild-image / --clean / --release）" >&2
      exit 2 ;;
  esac
done
# 默认目标
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=("app_pc_720" "app_pc_360")
# 去重
mapfile -t TARGETS < <(printf '%s\n' "${TARGETS[@]}" | awk '!seen[$0]++')

command -v docker >/dev/null 2>&1 || { echo "需要 docker，但没找到。" >&2; exit 1; }

# ---- 1) 构建镜像 -----------------------------------------------------------
if [[ "$REBUILD_IMAGE" -eq 1 ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo ">>> 构建 docker 镜像 $IMAGE ..."
  docker build -t "$IMAGE" -f "$DOCKERFILE" .
else
  echo ">>> 复用已存在的 docker 镜像 $IMAGE（--rebuild-image 可强制重建）"
fi

# ---- 2) 容器内交叉编译 + 收集 DLL ------------------------------------------
[[ "$CLEAN" -eq 1 ]] && { echo ">>> 清理 $BUILD_DIR ..."; rm -rf "$BUILD_DIR"; }

TARGET_ARGS="${TARGETS[*]}"
CMAKE_EXTRA_ARGS="${CMAKE_EXTRA[*]:-}"

echo ">>> 交叉编译目标: $TARGET_ARGS"
docker run --rm -v "$PWD":/work -w /work "$IMAGE" bash -euo pipefail -c '
  cmake -S . -B '"$BUILD_DIR"' -DTARGET_PC=ON \
        --toolchain '"$TOOLCHAIN"' '"$CMAKE_EXTRA_ARGS"'
  cmake --build '"$BUILD_DIR"' --target '"$TARGET_ARGS"' -j'"${JOBS:-$(nproc)}"'
  for t in '"$TARGET_ARGS"'; do
    MINGW_SYSROOT=/mingw64 bash scripts/collect-dll.sh '"$BUILD_DIR"'/$t.exe
  done
'

# ---- 3) 打包到 dist/（只留运行时文件，剥掉一堆中间编译产物）----------------
# 360/720 只差一个编译宏，DLL/res/pcdata 完全一样 —— 两个 exe 共放同一目录，
# 依赖只存一份。整目录拷到 Windows，按分辨率双击对应 exe 即可。
echo ">>> 打包 -> $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
for t in "${TARGETS[@]}"; do
  cp "$BUILD_DIR/$t.exe" "$DIST_DIR/"
done
cp "$BUILD_DIR"/*.dll "$DIST_DIR/" 2>/dev/null || true
[[ -d "$BUILD_DIR/res" ]]    && cp -r "$BUILD_DIR/res"    "$DIST_DIR/"
[[ -d "$BUILD_DIR/pcdata" ]] && cp -r "$BUILD_DIR/pcdata" "$DIST_DIR/"

# ---- 4) 汇总 ---------------------------------------------------------------
echo ""
echo ">>> 完成。运行时产物（整目录拷到 Windows，按分辨率双击对应 exe）:"
echo "    $DIST_DIR/"
for t in "${TARGETS[@]}"; do
  echo "      ├─ $t.exe"
done
echo "      ├─ *.dll  (共用)"
echo "      ├─ res/   (共用)"
echo "      └─ pcdata/(共用)"
