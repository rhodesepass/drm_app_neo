#!/usr/bin/env bash
#
# 跑一个 fuzz target。崩溃/超时/OOM 样本落在 fuzz/findings/<target>_ 前缀下,
# 用 fuzz/<target> <样本文件> 可单独复现。
#
# 用法:
#   fuzz/run.sh epconfig [秒数]  默认 300
#
# 解码栈(mp4/h264/h265)的 fuzz 已随 srgnvdec 包走: 见 ../srgnvdec/fuzz/run.sh
#
# 复现单个样本:  fuzz/fuzz_epconfig fuzz/findings/epconfig_crash-xxxx
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
T="${1:?用法: run.sh <epconfig> [秒数]}"
SECS="${2:-300}"

case "$T" in
  epconfig) BIN="$HERE/fuzz_epconfig"; CORP="$HERE/corpus_epconfig"; MAXLEN=8192   ;;
  *) echo "未知 target: $T"; exit 1 ;;
esac
[ -x "$BIN" ] || { echo "先跑 fuzz/build.sh"; exit 1; }

# 语料目录为空时自动生成起始种子(完整变异语料不入库, 见 seed_corpus.sh)。
if [ ! -d "$CORP" ] || [ -z "$(ls -A "$CORP" 2>/dev/null)" ]; then
  echo "[*] corpus 为空, 运行 seed_corpus.sh $T"
  "$HERE/seed_corpus.sh" "$T"
fi

# -timeout: 单次执行超过即判超时(抓死循环); -rss_limit_mb: 抓内存爆炸。
# 设备只有 64MB, 这里给 512MB 作 host 侧上限, 触到即视为内存 DoS。
exec timeout "$SECS" "$BIN" \
    -max_len="$MAXLEN" -timeout=8 -rss_limit_mb=512 \
    -artifact_prefix="$HERE/findings/${T}_" \
    "$CORP"
