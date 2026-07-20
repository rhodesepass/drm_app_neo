#!/usr/bin/env bash
#
# 生成 fuzz 起始种子语料(小、可读、覆盖校验分支)。
#
# 完整变异语料(corpus_* 里那几千个 hash 文件)不必入库 —— 跑 fuzzer 会自己长出来。
# 入库的是: 本脚本 + regressions/(崩溃复现样本)。
#
# 用法:
#   fuzz/seed_corpus.sh              # 往 corpus_* 写入命名种子(不删已有变异样本)
#   fuzz/seed_corpus.sh --reset      # 清空 corpus_* 后只留种子(丢掉本地变异语料)
#   fuzz/seed_corpus.sh epconfig     # 只生成某个 target
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
RESET=0
ONLY=""

for arg in "$@"; do
  case "$arg" in
    --reset) RESET=1 ;;
    h264|mp4|epconfig) ONLY="$arg" ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "未知参数: $arg"; exit 1 ;;
  esac
done

want() { [ -z "$ONLY" ] || [ "$ONLY" = "$1" ]; }

write_file() {
  # write_file <path> <contents via stdin>
  local path="$1"
  mkdir -p "$(dirname "$path")"
  cat >"$path"
}

# ---------------------------------------------------------------------------
# epconfig: 手写 JSON, 对准 operators.c 各校验早退/分支
# ---------------------------------------------------------------------------
seed_epconfig() {
  local d="$HERE/corpus_epconfig"
  [ "$RESET" = 1 ] && rm -rf "$d"
  mkdir -p "$d"

  write_file "$d/00_empty"               </dev/null
  write_file "$d/01_not_json"            <<<"not json at all"
  write_file "$d/02_empty_obj"           <<<'{}'
  write_file "$d/03_bad_version"         <<<'{"version":99,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"}}'
  write_file "$d/04_version_string"      <<<'{"version":"1","uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"}}'
  write_file "$d/05_missing_uuid"        <<<'{"version":1,"screen":"360x640","loop":{"file":"x.mp4"}}'
  write_file "$d/06_bad_uuid"            <<<'{"version":1,"uuid":"not-a-uuid","screen":"360x640","loop":{"file":"x.mp4"}}'
  write_file "$d/07_missing_screen"      <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","loop":{"file":"x.mp4"}}'
  write_file "$d/08_bad_screen"          <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"1080x1920","loop":{"file":"x.mp4"}}'
  write_file "$d/09_missing_loop"        <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640"}'
  write_file "$d/10_loop_not_obj"        <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":"x.mp4"}'
  write_file "$d/11_missing_loop_file"   <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{}}'
  # loop.file 指向不存在的文件 → harness 临时目录里必然失败, 覆盖该错误路径
  write_file "$d/12_loop_file_missing"   <<<'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","name":"seed","screen":"360x640","loop":{"file":"no_such.mp4"}}'

  write_file "$d/20_intro_enabled_no_file" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"intro":{"enabled":true,"duration":1000}}
EOF
  write_file "$d/21_intro_bad_duration" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"intro":{"enabled":true,"duration":0,"file":"i.mp4"}}
EOF

  write_file "$d/30_tr_bad_type" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"explode"}}
EOF
  write_file "$d/31_tr_no_options" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"fade"}}
EOF
  write_file "$d/32_tr_bad_duration" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"fade","options":{"duration":0}}}
EOF
  write_file "$d/33_tr_fade_ok_shape" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","name":"t","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"fade","options":{"duration":200000,"background_color":"#000000"}}}
EOF
  write_file "$d/34_tr_move" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"move","options":{"duration":100,"background_color":"#ffffff"}}}
EOF
  write_file "$d/35_tr_swipe" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"swipe","options":{"duration":100}}}
EOF
  write_file "$d/36_tr_none" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"transition_in":{"type":"none"}}
EOF

  write_file "$d/40_overlay_ark_no_opt" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"overlay":{"type":"arknights"}}
EOF
  write_file "$d/41_overlay_custom_empty" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"overlay":{"type":"custom","options":{"appear_time":0,"duration":1,"elements":[]}}}
EOF
  write_file "$d/42_overlay_bad_el_type" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"overlay":{"type":"custom","options":{"appear_time":0,"duration":1,"elements":[{"type":"spaceship"}]}}}
EOF
  write_file "$d/43_overlay_barcode_no_text" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"overlay":{"type":"custom","options":{"appear_time":0,"duration":1,"elements":[{"type":"barcode","x":0,"y":0,"w":10,"h":10}]}}}
EOF
  write_file "$d/44_overlay_rect_no_wh" <<'EOF'
{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"},"overlay":{"type":"custom","options":{"appear_time":0,"duration":1,"elements":[{"type":"rect","x":0,"y":0}]}}}
EOF
  write_file "$d/45_overlay_rich" <<'EOF'
{
  "version": 1,
  "uuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "name": "seed-rich",
  "description": "custom overlay branches",
  "icon": "missing.png",
  "screen": "360x640",
  "loop": { "file": "x.mp4" },
  "transition_in": {
    "type": "fade",
    "options": { "duration": 200000, "background_color": "#000000", "image": "missing.jpg" }
  },
  "overlay": {
    "type": "custom",
    "options": {
      "appear_time": 100000,
      "duration": 800000,
      "elements": [
        { "type": "text", "x": 1, "y": 2, "text": "A", "font": "display", "font_size": 14,
          "color": "#FFFFFF", "animation": "typewriter", "start_frame": 0, "speed": 2, "end_frame": 10 },
        { "type": "rect", "x": 0, "y": 0, "w": 10, "h": 10, "color": "#FFFFFF",
          "animation": "move", "from_dx": -1, "from_dy": 0, "start_frame": 0, "speed": 1 },
        { "type": "barcode", "x": 0, "y": 0, "w": 8, "h": 40, "text": "SEED",
          "animation": "eink", "start_frame": 0, "speed": 1 },
        { "type": "corner_fade", "color": "#112233", "w": 32, "animation": "grow",
          "start_frame": 0, "speed": 1 },
        { "type": "text_rot90", "x": 0, "y": 0, "w": 10, "h": 40, "text": "R",
          "font": "display", "font_size": 12, "color": "#FFFFFF", "animation": "none" },
        { "type": "image", "image": "no.png", "x": 0, "y": 0, "animation": "fade",
          "start_frame": 0, "speed": 1 }
      ]
    }
  }
}
EOF

  # 从仓库真实配置拷一份(合法形状, 仍会在 loop.file 处因文件不存在早退)
  if [ -f "$ROOT/assets/fallback/epconfig.json" ]; then
    cp -f "$ROOT/assets/fallback/epconfig.json" "$d/90_fallback_epconfig.json"
  fi
  if [ -f "$ROOT/testdata/opinfo_custom/epconfig.json" ]; then
    cp -f "$ROOT/testdata/opinfo_custom/epconfig.json" "$d/91_opinfo_custom_epconfig.json"
  fi

  # 畸形: 深嵌套 / 超长键 / 截断
  python3 - <<'PY' "$d"
import json, sys
from pathlib import Path
d = Path(sys.argv[1])
deep = {"version": 1, "uuid": "3de46ad7-11a1-4eda-bb64-a692f5873a53",
        "screen": "360x640", "loop": {"file": "x.mp4"}, "extra": {}}
cur = deep["extra"]
for i in range(40):
    cur["n"] = {}
    cur = cur["n"]
(d / "50_deep_nest.json").write_text(json.dumps(deep), encoding="utf-8")
(d / "51_long_key.json").write_text(
    json.dumps({"version": 1, "uuid": "3de46ad7-11a1-4eda-bb64-a692f5873a53",
                "screen": "360x640", "loop": {"file": "x.mp4"}, "k" * 4000: "v"}),
    encoding="utf-8")
(d / "52_truncated.json").write_bytes(
    b'{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":')
(d / "53_bom.json").write_bytes(
    b'\xef\xbb\xbf{"version":1,"uuid":"3de46ad7-11a1-4eda-bb64-a692f5873a53","screen":"360x640","loop":{"file":"x.mp4"}}')
(d / "54_null_bytes.bin").write_bytes(b'{"version":1,\x00"uuid":"x"}')
PY

  echo "[+] epconfig seeds -> $d ($(ls -1 "$d" | wc -l) files)"
}

# ---------------------------------------------------------------------------
# mp4: 最小合法壳 + 畸形 box(对准 stsz/stco/size 溢出类路径)
# ---------------------------------------------------------------------------
seed_mp4() {
  local d="$HERE/corpus_mp4"
  [ "$RESET" = 1 ] && rm -rf "$d"
  mkdir -p "$d"

  python3 - <<'PY' "$d"
import struct, sys
from pathlib import Path
d = Path(sys.argv[1])

def box(typ: bytes, payload: bytes) -> bytes:
    assert len(typ) == 4
    return struct.pack(">I", 8 + len(payload)) + typ + payload

def ftyp() -> bytes:
    # major=isom, minor=0, compat=isom/iso2/mp41
    return box(b"ftyp", b"isom" + struct.pack(">I", 0) + b"isomiso2mp41")

# 最小: 只有 ftyp
(d / "00_ftyp_only.mp4").write_bytes(ftyp())

# ftyp + 空 mdat
(d / "01_ftyp_mdat.mp4").write_bytes(ftyp() + box(b"mdat", b"\x00" * 16))

# 巨 size 字段(触发 box 迭代指针算术溢出类路径; 已修)
(d / "02_huge_size.mp4").write_bytes(ftyp() + struct.pack(">I", 0xFFFFFFFF) + b"mdat" + b"\x00" * 8)

# size=1 的 64-bit largesize 声明但数据不够
(d / "03_largesize_trunc.mp4").write_bytes(
    ftyp() + struct.pack(">I", 1) + b"mdat" + struct.pack(">Q", 1 << 40))

# size=0 (至 EOF)
(d / "04_size_zero.mp4").write_bytes(ftyp() + struct.pack(">I", 0) + b"mdat" + b"ABCD")

# 畸形 stsz: sample_count 巨大
stsz = struct.pack(">I", 0) + struct.pack(">I", 0) + struct.pack(">I", 0x0FFFFFFF)  # ver/flags, default, count
(d / "05_stsz_huge_count.mp4").write_bytes(ftyp() + box(b"stsz", stsz))

# 畸形 stco: entry_count 巨大
stco = struct.pack(">I", 0) + struct.pack(">I", 0xFFFFFFFF)
(d / "06_stco_huge_count.mp4").write_bytes(ftyp() + box(b"stco", stco))

# 嵌套 moov/trak/mdia/minf/stbl 空壳
stbl = box(b"stbl", box(b"stsd", struct.pack(">I", 0) + struct.pack(">I", 0))
                 + box(b"stts", struct.pack(">I", 0) + struct.pack(">I", 0))
                 + box(b"stsc", struct.pack(">I", 0) + struct.pack(">I", 0))
                 + box(b"stsz", struct.pack(">I", 0) + struct.pack(">I", 0) + struct.pack(">I", 0))
                 + box(b"stco", struct.pack(">I", 0) + struct.pack(">I", 0)))
minf = box(b"minf", box(b"vmhd", struct.pack(">I", 0) + struct.pack(">HH", 0, 0) + struct.pack(">I", 0))
                 + box(b"dinf", b"") + stbl)
mdia = box(b"mdia", box(b"mdhd", b"\x00" * 24) + box(b"hdlr", b"\x00" * 8 + b"vide" + b"\x00" * 12)
                 + minf)
trak = box(b"trak", box(b"tkhd", b"\x00" * 84) + mdia)
moov = box(b"moov", box(b"mvhd", b"\x00" * 100) + trak)
(d / "07_empty_moov.mp4").write_bytes(ftyp() + moov)

# 截断
(d / "08_trunc_header.mp4").write_bytes(b"\x00\x00\x00\x20ft")
(d / "09_all_ff.bin").write_bytes(b"\xff" * 64)
PY

  # 可选: 从仓库素材截一小段真实 mp4(有 ffmpeg 才做)
  local src="$ROOT/assets/fallback/loop_1.mp4"
  if [ -f "$src" ] && command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -y -loglevel error -i "$src" -t 0.1 -c copy -an "$d/90_loop_snip.mp4" || true
  elif [ -f "$HERE/corpus_mp4/loop_short.mp4" ]; then
    : # 已有
  fi
  # 若已有命名短样本则保留
  if [ -f "$ROOT/assets/fallback/loop_1.mp4" ] && [ ! -f "$d/90_loop_snip.mp4" ]; then
    # 无 ffmpeg 时只拷文件头, 足够让 demuxer 走进真实路径
    head -c 32768 "$ROOT/assets/fallback/loop_1.mp4" >"$d/90_loop_head.mp4"
  fi

  echo "[+] mp4 seeds -> $d ($(ls -1 "$d" | wc -l) files)"
}

# ---------------------------------------------------------------------------
# h264: 裸 NAL / 长度前缀 / 已知崩溃形状(回归已另存 regressions/)
# ---------------------------------------------------------------------------
seed_h264() {
  local d="$HERE/corpus_h264"
  [ "$RESET" = 1 ] && rm -rf "$d"
  mkdir -p "$d"

  python3 - <<'PY' "$d"
import struct, sys
from pathlib import Path
d = Path(sys.argv[1])

# 空 / 极短
(d / "00_empty").write_bytes(b"")
(d / "01_one_byte").write_bytes(b"\x00")
(d / "02_ff8").write_bytes(b"\xff" * 8)          # 曾触发 nalu 整数溢出

# Annex-B start codes
(d / "10_annexb_zeros").write_bytes(b"\x00\x00\x00\x01\x00")
(d / "11_annexb_sps_like").write_bytes(
    b"\x00\x00\x00\x01\x67" + bytes([0x42, 0x00, 0x0a, 0xf8, 0x41, 0xa2])
    + b"\x00\x00\x00\x01\x68" + bytes([0xce, 0x3c, 0x80])
    + b"\x00\x00\x00\x01\x65" + b"\x88" * 32)

# AVCC 长度前缀: 4 字节大端长度 + NAL
def avcc(nal: bytes) -> bytes:
    return struct.pack(">I", len(nal)) + nal

(d / "20_avcc_tiny").write_bytes(avcc(b"\x67\x42\x00\x0a") + avcc(b"\x65\x00"))
(d / "21_avcc_len_overflow").write_bytes(b"\xff\xff\xff\xff" + b"\x65\x00")
(d / "22_avcc_len_too_big").write_bytes(struct.pack(">I", 0x100000) + b"\x65" + b"\x00" * 8)

# 截断码流(触发 bitreader 耗尽脏状态)
(d / "30_trunc_expgolomb").write_bytes(b"\x00\x00\x00\x01\x67\xff")

# 随机垃圾让 mutator 有材料
(d / "40_noise").write_bytes(bytes(range(256)))
PY

  # 从真实 mp4 抽 annex-B(可选)
  local src="$ROOT/assets/fallback/loop_1.mp4"
  if [ -f "$src" ] && command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -y -loglevel error -i "$src" -t 0.1 -c:v copy -bsf:v h264_mp4toannexb \
      -an -f h264 "$d/90_loop_snip.h264" || true
  fi

  # 把 regressions 也放进语料, 保证每次 fuzz 都覆盖已修路径
  if [ -d "$HERE/regressions" ]; then
    for f in "$HERE/regressions"/h264_*.bin; do
      [ -f "$f" ] || continue
      cp -f "$f" "$d/99_$(basename "$f")"
    done
  fi

  echo "[+] h264 seeds -> $d ($(ls -1 "$d" | wc -l) files)"
}

want epconfig && seed_epconfig
want mp4      && seed_mp4
want h264     && seed_h264

echo "[+] done. 接着: fuzz/build.sh && fuzz/run.sh <target>"
echo "    提示: 长跑后可用 merge 精简语料(不必整包入库):"
echo "      ./fuzz_epconfig -merge=1 fuzz/corpus_epconfig_min fuzz/corpus_epconfig"
