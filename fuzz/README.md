# drm_app_neo 解析器 Fuzzing

对设备上处理**外部不可信素材**的解析器做 fuzz。这些代码与硬件无关(只吃字节),
所以直接在 host 上用 clang libFuzzer + ASan + UBSan 编译运行,不需要交叉工具链或上板。

**解码栈(mp4 demux / H.264 / H.265 解析)已抽成独立包 srgnvdec**,它的 fuzz
全家桶(fuzz_mp4 / fuzz_h264 / fuzz_h264_f1c / fuzz_h265 + 回归样本)随包维护,
见 `../srgnvdec/fuzz/`。历史上由这套 harness 发现并修复的 9 个解析器问题
(整数溢出/死循环/OOM/移位 UB 等)的记录也在那边的 README 里。这里只剩 app
自己的解析面。

## 目标

| target | 被测代码 | 覆盖的攻击面 |
|---|---|---|
| `fuzz_epconfig` | `src/prts/operators.c`(+ misc/cJSON/uuid) | `epconfig.json` 每个字段的校验逻辑 |

`fuzz_epconfig` 用 `fuzz/stubs/` 里的桩头顶掉 lvgl/字体/overlay 依赖(解析路径不碰它们),
只保留纯校验逻辑。

## 用法

```bash
fuzz/seed_corpus.sh        # 生成起始种子(语料空时 run.sh 也会自动调)
fuzz/build.sh
fuzz/run.sh epconfig 300   # 跑 N 秒(默认 300)

# 复现单个崩溃样本:
fuzz/fuzz_epconfig fuzz/findings/epconfig_crash-xxxx
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
