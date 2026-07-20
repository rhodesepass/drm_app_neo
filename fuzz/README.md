# drm_app_neo 解析器 Fuzzing

对设备上处理**外部不可信素材**的解析器做 fuzz。这些代码全部与硬件无关(只吃字节),
所以直接在 host 上用 clang libFuzzer + ASan + UBSan 编译运行,不需要交叉工具链或上板。

## 目标

| target | 被测代码 | 覆盖的攻击面 |
|---|---|---|
| `fuzz_mp4` | `src/vdec/mp4_demux.c` | ISO-BMFF box 遍历、stsz/stco/stsc 采样表 —— 加载的 mp4 文件 |
| `fuzz_h264` | `src/vdec/{nalu,bitreader,h264_parser}.c` | NAL 切分、Exp-Golomb 位流、SPS/PPS/slice 头 —— mp4 里的码流 |
| `fuzz_epconfig` | `src/prts/operators.c`(+ misc/cJSON/uuid) | `epconfig.json` 每个字段的校验逻辑 |

`fuzz_epconfig` 用 `fuzz/stubs/` 里的桩头顶掉 lvgl/字体/overlay 依赖(解析路径不碰它们),
只保留纯校验逻辑。

## 用法

```bash
fuzz/seed_corpus.sh        # 生成起始种子(语料空时 run.sh 也会自动调)
fuzz/build.sh              # 编三个 target
fuzz/run.sh h264 300       # 跑某个 target N 秒(默认 300)
fuzz/run.sh mp4
fuzz/run.sh epconfig

# 复现单个崩溃样本:
fuzz/fuzz_h264 fuzz/findings/h264_crash-xxxx
# 或已入库的回归样本:
fuzz/fuzz_h264 fuzz/regressions/h264_nalu_intoverflow.bin
```

崩溃/超时/OOM 样本落在 `fuzz/findings/<target>_` 前缀下。工作语料在 `fuzz/corpus_*/`(gitignore,不入库)。

### 语料要不要入库?

| 内容 | 入库? | 原因 |
|---|---|---|
| `seed_corpus.sh` 生成的命名种子 | 否(脚本可再生成) | 几十个小文件,脚本可复现 |
| `corpus_*` 变异长大后的 hash 样本 | **否** | 易到几十 MB / 上万文件,跑 fuzzer 会再长出来 |
| `regressions/` 崩溃复现样本 | **是** | 保证修复不被回归;体积可忽略 |

需要把某次长跑的覆盖率固化时,用 merge 精简后再自行决定是否归档,不要整包 commit:

```bash
mkdir -p fuzz/corpus_epconfig_min
./fuzz/fuzz_epconfig -merge=1 fuzz/corpus_epconfig_min fuzz/corpus_epconfig
```

## 已发现并修复的问题(2026-07,首轮)

均由本套 harness 发现,已修,并用原崩溃样本验证不再触发。全部是**畸形素材可远程触发**,
设备只有 64MB RAM,DoS/OOM 尤其致命。

| # | 位置 | 类型 | 后果 | 触发 |
|---|---|---|---|---|
| 1 | `nalu.c` `nalu_next_length_prefixed` | 整数溢出绕过边界检查 | 越界读(伪 NAL 指向缓冲区外) | 8 字节 `FF..` |
| 2 | `h264_parser.c` `parse_one_rplm` | 无终止循环 | 解码线程 100% CPU 永久卡死 | 390 字节切片头 |
| 3 | `mp4_demux.c` stsz 采样数 | 无上限 `calloc` | `malloc(6.7GB)` → OOM 崩溃 | 几字节 stsz |
| 4 | `h264_parser.c` `parse_pred_weight_table` | 有符号移位 UB | `1<<denom`(denom 可达 2^32) | 畸形 pred_weight |
| 5 | `bitreader.c` `br_read` | 数据耗尽留脏状态 → 移位溢出 UB | 后续读 `shift>=8` 移位溢出 | 截断码流 |
| 6 | `mp4_demux.c` box 迭代 | 指针算术溢出绕过检查 | `p+size` 回绕 → 越界读 | 巨 size 字段的 box |
| 7 | `h264_parser.c` `compute_poc` | 有符号移位 UB | `1<<(log2_max_poc_lsb+4)` 移位 259 位 | SPS log2 字段越界 |
| 8 | `mp4_demux.c` `build_samples` | chunk_count 无上限 | stsc 遍历 40 亿次死循环卡死 | 巨 chunk_count 的 stco |
| 9 | `h264_parser.c` `compute_poc` | 有符号整数溢出 UB | `cycle_cnt*expected_delta` 等 POC 链式算术溢出 | 畸形 SPS offset 字段 |

问题 4、7 同源(SPS/slice 里若干本该在 0..7/0..12 的字段没按规范钳制就参与移位);
问题 3、8 同源(mp4 采样表里的 32 位计数字段没拿文件尺寸封顶);
问题 9 是 POC(picture order count)计算全程用 int32 链式乘加, 畸形值溢出 —— POC 本是模运算,
改用 int64 中间量算完再截回。修复都在源头钳/封/加宽。

发现节奏: 前 6 个几十秒内就出, 7~9 号要跑到 5~8 分钟才浮现(覆盖率从 340 爬到 436 后),
所以回归时每个 target 至少跑 10 分钟才算过。

`epconfig.json` 的字段校验全程干净(累计 300 万+次执行),没发现问题 —— 那条路径的校验写得很稳。

## Monkey test (整机随机按键)

fuzz 打的是单个解析器的输入; monkey 打的是**整个 app 的状态机** —— 随机导航把 UI 各屏、
动画中断、快速连按这些人测不到的角落走一遍, 全程挂 ASan+UBSan, 断言只有"别崩别 UB"。

```bash
# 一次性建 sanitizer 版 app (见 monkey.sh 顶部 BUILD 注释), 然后:
fuzz/monkey.sh 90 4        # 4 个种子各 90s
```

- 随机键由 `EPASS_MONKEY="<seed>:<step_ms>:<dur_ms>"` 驱动, PRNG 种子=seed, **崩溃 100% 可复现**。
- 无头(SDL dummy), 到 dur_ms 自动退出。
- 编译务必带 `-fno-sanitize=function`: 否则 LVGL 回调惯用的函数指针类型转换会刷屏误报。
- 首轮 4 种子各 90s 全干净(此前 9 个解析器 bug 修复后)。

## 备注

- 崩溃归类:`crash-`=ASan/UBSan,`timeout-`=死循环(单次执行 >8s),`oom-`=内存超限(>512MB)。
- `-rss_limit_mb=512` 是 host 侧上限;设备实际只有 64MB,任何触到 host 上限的输入在设备上更早死。
- 后续可加的 target:`apps_cfg_parse.c`(appconfig.json)、图片解码(stb_image/libpng/tjpgd)。
- **以上 9 个修复都只在 host fuzzer 里验证过, 尚未上板实测。** 均为纯解析逻辑(边界/钳制/
  整型),不涉硬件寄存器, 但合入设备固件前建议至少放真实素材回归一遍确认没改坏正常解码。
