#pragma once
#include "icons.h"

// ========== Resource Layout ==========
// 内置资源放在可执行文件同级的 RES_SUBDIR 子目录, 运行时由 respath 模块解析。
// CMake 按本结构把仓库 assets/ 拷过去: 根 *.png, fonts/*.otf, fallback/。
#define RES_SUBDIR "res"
#define RES_FONTS_SUBDIR "fonts"
#define RES_DEFAULT_ICON_FILE "defaulticon.png"

// ========== Application Information ==========
#define APP_SUBCODENAME "a10bydesign"
#define APP_BARNER \
    "           =-+           \n" \
    "     +@@@@@  @@@@@@        Rhodes Island\n" \
    "     +@*.:@@ @@  %@-       Electronic Pass\n" \
    "     *@@@%:  @@@@@=:       DRM System\n" \
    "   ==+@      @@  @@ .=   \n" \
    "  +                   *    CodeName:\n" \
    "   ==%@@@@@@ %@@@@@ .+     " APP_SUBCODENAME "\n" \
    "     :  @=   %@%.  -       \n" \
    "       :@=      +@@        Rhodes Island\n" \
    "        @#   %@@@@@        Engineering Dept.\n" \
    "           =:=        \n"

#define APP_ABOUT_MSG \
    "基于LVGL和寄存器魔法的方舟通行证展示程序\n" \
    "电子通行证 Contributers 2026 GPLV3\n" \
    "白银 伊卡洛斯sama 薄云 Et al.\n" \
    "https://github.com/rhodesepass\n" \
    "电子通行证是白银个人业余时间设计的一款开源的自由硬件，人人均可复刻，" \
    "与鹰角网络没有任何直接或间接的关联。相关游戏素材版权属于鹰角网络。\n"\
    "本设计方案按“原样”提供，不附带任何形式的明示或默示担保。白银不对使用本设计方案造成的任何索赔、损害或其他责任承担责任。" 

#define APP_VERSION EPASS_GIT_TAG
#define APP_VERSION_STRING (APP_VERSION "_" EPASS_GIT_VERSION)

// ========== Settings Configuration ==========
#define SETTINGS_FILE_PATH "/root/epass_cfg.bin"
#define SETTINGS_MAGIC 0x45504153434F4E46
#define SETTINGS_VERSION 4
#define SETTINGS_BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"

// 文件管理器上次浏览目录 (lv_fs 带盘符路径, 如 "A:/root/xxx/")。跨重启恢复。
#define FILEMANAGER_ROOT_DIR "A:/root/"
#define FILEMANAGER_LAST_DIR_FILE "/root/epass_fm_lastdir.txt"

// ========== Storage Configuration ==========
#define NAND_MOUNT_POINT "/"
#define SD_MOUNT_POINT "/sd"
// NAND 启动时外置 SD 卡数据分区是 mmcblk0p1;
// SD 启动时系统本身在 SD 上(root=mmcblk0p2), /sd 对应第三个 share 分区
// (S000sdsetup 负责格式化)。运行时由 sd_dev_path() 按 cmdline 选择。
#define SD_BOOT_CMDLINE_SIGN  "root=/dev/mmcblk0p2"
#define SD_DEV_PATH_NAND_BOOT "/dev/mmcblk0p1"
#define SD_DEV_PATH_SD_BOOT   "/dev/mmcblk0p3"

// ========== PRTS Configuration ==========
#define PRTS_OPERATORS_MAX 256
#define PRTS_TIMER_MAX 1024
#define PRTS_OPERATOR_PARSE_LOG "/root/asset.log"
#define PRTS_ORDER_FILE "/root/epass_oporder.txt"
#define PRTS_ASSET_VERSION_NUMBER 1
#define PRTS_ASSET_CONFIG_FILENAME "epconfig.json"
#define PRTS_ASSET_DIR "/assets/"
#define PRTS_ASSET_DIR_SD SD_MOUNT_POINT "/assets/"
#define PRTS_TICK_PERIOD (1000 * 1000)
// 相对 res/ 的内置资源 (运行时经 respath()/respath_lvfs() 解析到可执行文件同级)。
#define PRTS_FALLBACK_ASSET_SUBDIR "fallback"

// ========== Apps Configuration ==========
#define APPS_MAX 64
#define APPS_EXTMAP_MAX 128
#define APPS_PARSE_LOG "/root/apps.log"
#define APPS_CONFIG_VERSION 2
#define APPS_CONFIG_FILENAME "appconfig.json"
#define APPS_DIR "/app/"
#define APPS_DIR_SD SD_MOUNT_POINT "/app/"
#define APPS_BG_APP_CHECK_PERIOD (1000 * 1000)
#define APPS_BG_KILL_TIMEOUT_US (1000 * 1000)
#define APPS_IPC_SOCKET_PATH "/tmp/epass_drm_app.sock"
#define APPS_IPC_MAX_MSG 512
#define APPS_IPC_BACKLOG 16


// ========== Screen Configuration ==========
// 屏目标由构建系统注入：cmake -DEPASS_SCREEN=360x640 | 480x854 | 720x1280
// (设备 CMakeLists 与 sim/CMakeLists 都支持该变量)。
// 未注入时兜底默认 360x640 (F1C200s 主目标)，保证独立/clangd 也能解析。
#if !defined(USE_360_640_SCREEN) && !defined(USE_480_854_SCREEN) && !defined(USE_720_1280_SCREEN)
#define USE_360_640_SCREEN
#endif

// resolution alternatives.
// 设计基准为 360x640 (UI_SCALE=1)。T113 为整数 2x (UI_SCALE=2)，
// 同比例 9:16，所有坐标/尺寸均为基准值的整数倍，详见 ui_metrics.h 的 S()。
#if defined(USE_360_640_SCREEN)
    // 单一缩放系数：F1C200s 基准档
    #define UI_SCALE 1

    #define VIDEO_WIDTH 384
    #define VIDEO_HEIGHT 640

    // 兼容 720x1280 时代高清素材：真实内容 720x1280，编码宽按 32 对齐补到 736。
    // 挂载时 src 裁窗取左 720x1280(丢掉右 16px 对齐 padding)，
    // 再由 DEFE frontend 硬件缩小到屏幕 360x640(等比 1/2)。
    // VE 解码耗时与 720 档相同(~20-26ms/帧)，capture buffer 数由
    // VDEC_CAPTURE_LARGE_AREA 按编码面积自动落到 LARGE 档。
    #define VIDEO_HIRES_WIDTH 736           // 对齐后编码宽
    #define VIDEO_HIRES_HEIGHT 1280
    #define VIDEO_HIRES_CROP_WIDTH 720      // 真实内容宽(裁窗)

    #define UI_WIDTH 360
    #define UI_HEIGHT 640
    #define OVERLAY_WIDTH 360
    #define OVERLAY_HEIGHT 640
    #define SCREEN_WIDTH 360
    #define SCREEN_HEIGHT 640

    #define UI_OPLIST_VISIBLE_SLOTS 8
    #define UI_OPLIST_ITEM_HEIGHT 80

    #define UI_APP_VISIBLE_SLOTS 12
    #define UI_APP_ITEM_HEIGHT 80


    // 干员列表和亮度设置的Y坐标
    #define UI_OPLIST_Y 250
    #define UI_SPINNER_INTRO_Y 580
    #define UI_MAINMENU_Y 190
    #define UI_WARNING_Y 565
    #define UI_CONFIRM_Y (UI_HEIGHT - 125)
    #define UI_USBSELECT_Y (UI_HEIGHT - 190)

#elif defined(USE_480_854_SCREEN)
    #error "USE_480_854_SCREEN is not supported yet!"
#elif defined(USE_720_1280_SCREEN)
    // 单一缩放系数：T113 档，整数 2x，全部为 360x640 基准的 2 倍
    #define UI_SCALE 2

    #define VIDEO_WIDTH 736
    #define VIDEO_HEIGHT 1280

    // 兼容 360x640 时代旧素材：真实内容 360x640，编码宽按 32 对齐补到 384(FB 按 384 分配)。
    // 挂载时先用 src 裁窗取左 360x640(丢掉右 24px 对齐 padding，避免其参与缩放采样)，
    // 再由 DEFE frontend 硬件放大到屏幕 720x1280(等比 2x)。
    // 仅 720 档定义；未定义时 legacy 路径整体不编译。
    #define VIDEO_LEGACY_WIDTH 384          // 对齐后编码宽，FB 分配用
    #define VIDEO_LEGACY_HEIGHT 640
    #define VIDEO_LEGACY_CROP_WIDTH 360     // 真实内容宽(裁窗)，高无 padding 用 VIDEO_LEGACY_HEIGHT

    #define UI_WIDTH 720
    #define UI_HEIGHT 1280
    #define OVERLAY_WIDTH 720
    #define OVERLAY_HEIGHT 1280
    #define SCREEN_WIDTH 720
    #define SCREEN_HEIGHT 1280

    // 可见槽位是数量，不随分辨率缩放；
    #define UI_OPLIST_VISIBLE_SLOTS 12
    #define UI_OPLIST_ITEM_HEIGHT 80

    #define UI_APP_VISIBLE_SLOTS 12
    #define UI_APP_ITEM_HEIGHT 80


    // 干员列表和亮度设置的Y坐标
    #define UI_OPLIST_Y 500
    #define UI_SPINNER_INTRO_Y 1160
    #define UI_MAINMENU_Y 380
    #define UI_WARNING_Y 1130
    #define UI_CONFIRM_Y (UI_HEIGHT - 250)
    #define UI_USBSELECT_Y (UI_HEIGHT - 380)

#endif // USE_360_640_SCREEN, USE_480_854_SCREEN, USE_720_1280_SCREEN

// ========== Overlay 信息叠层坐标 (360 基准 × UI_SCALE，9:16 整数倍缩放) ==========
// config.h 在 ui_metrics.h 的 S() 定义之前被 include，故此处直接乘 UI_SCALE。
// 左上角矩形 X 偏移
#define OVERLAY_ARKNIGHTS_RECT_OFFSET_X     (55 * UI_SCALE)
// 下方信息区 左偏移
#define OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X (70 * UI_SCALE)
// 干员名 Y 偏移
#define OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y   (415 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y (455 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y (475 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_LINE_WIDTH        (280 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y   (460 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y (481 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y (525 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_WIDTH  (50 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_HEIGHT (50 * UI_SCALE)
// 左下角 "- Arknights -" 矩形文字 Y 偏移
#define OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y   (577 * UI_SCALE)
// 辅助文字 Y 偏移
#define OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y (592 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_AUX_TEXT_LINE_HEIGHT (15 * UI_SCALE)
// 左下角条码 偏移
#define OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y  (450 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_BARCODE_WIDTH     (50 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_BARCODE_HEIGHT    (180 * UI_SCALE)
// 右上角装饰箭头 Y 偏移
#define OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y (100 * UI_SCALE)

// ========== DRM Warpper Layer Configuration ==========
#define DRM_WARPPER_LAYER_UI 2
#define DRM_WARPPER_LAYER_OVERLAY 1
#define DRM_WARPPER_LAYER_VIDEO 0

// overlay 层像素格式。1 = C8(256 色调色板, DEBE 片上 SRAM 查表出色,
// 每帧扫描 fetch 降为 8888 的 1/4, 省 DDR 带宽给 VE; 每个调色板项自带 8bit alpha),
// 0 = 回退 ARGB8888(A/B 对比与硬件翻车回滚用)。
// 需要内核 patch 0029(DRM_FORMAT_C8 + /sys/kernel/debe_palette/palette)。
// 硬件约束: 调色板全局唯一(四层共享一块 SRAM); C8 不能缩放(overlay 本不缩放);
// C8 恒占 alpha plane 名额(NV12+C8+RGB565 组合够用)。
#define OVERLAY_USE_C8 1

#if OVERLAY_USE_C8
#define OVERLAY_BPP 1
#else
#define OVERLAY_BPP 4
#endif
#define OVERLAY_BUF_BYTES (OVERLAY_WIDTH * OVERLAY_HEIGHT * OVERLAY_BPP)

#define DEBE_PALETTE_SYSFS_PATH "/sys/kernel/debe_palette/palette"

// ---------- C8 调色板分段(见 src/render/c8pal.h) ----------
// 烘焙段 0..95 编译期固定(c8pal_baked.h, tools/gen_c8.sh 生成):
//   0 透明 | 1 黑 | 2 白 | 3..10 白 alpha ramp | 11..18 黑 alpha ramp |
//   19..32 灰阶 14 级 | 33..95 装饰素材聚类 63 项
// 动态段 96..254:每个 owner(opinfo/transition)在 load 阶段把自己需要的
// 全部颜色攒进 params 的"颜色池",show 入口在层离屏时恢复烘焙段+写池+commit。
// 落盘走 LUT 最近反查,只认颜色不认位置,池内顺序无所谓。
// 255 = 反查 LUT sentinel, 永不分配。
#define C8PAL_BAKED_COUNT        96
#define C8PAL_IDX_TRANSPARENT    0
#define C8PAL_IDX_BLACK          1
#define C8PAL_IDX_WHITE          2

#define C8PAL_DYN_BASE           96
#define C8PAL_DYN_QUOTA          159
// image 模式全屏只有单图, 颜色池独占 1..254(连烘焙段一起覆盖也无妨,
// 下一个 owner 的 show 入口会先恢复烘焙段)
#define C8PAL_IMAGE_BASE         1
#define C8PAL_IMAGE_QUOTA        254

// 各类运行时量化的单图配额(写进 .c8pal 缓存头,改了会触发重算)
#define C8PAL_QUOTA_CLASS        16  // arknights 职业图标
#define C8PAL_QUOTA_AKLOGO       24  // arknights 右下 logo
#define C8PAL_QUOTA_TRIMG        32  // transition 图片
#define C8PAL_QUOTA_CUSTOM_IMG   32  // custom 元素图片(上限;池余量不足时取余量)
// theme color 的 alpha ramp(arknights corner fade)。通用反查 LUT 的 alpha 只有
// 16 桶,分不清 32 级——fade 走专用路径(c8pal_find_exact 定位 + Bayer 层间抖动)
#define C8PAL_THEME_RAMP_LEVELS  32
#define C8PAL_COLOR_RAMP_LEVELS  8   // custom 元素色的迷你 ramp(1 opaque + 8 alpha)
#define C8PAL_COLOR_RAMPS_MAX    4   // 池里最多几个元素色带 ramp,超出只写 opaque

// ========== Media Player (V4L2 stateless / cedrus) ==========
// OUTPUT(码流)buffer 大小：一帧一个 NAL，打开时按 mp4 最大 sample 校验
#define VDEC_OUTPUT_BUF_SIZE (512 * 1024)
#define VDEC_OUTPUT_BUF_COUNT 2
// B 帧重排深度下限（素材 has_b_frames=2）
#define VDEC_REORDER_DEPTH 2
// capture(解码帧) 数：有 VUI bitstream_restriction 时用编码器承诺的
// DPB 联合上限(max_dec_frame_buffering + bump滞后1 + 入队1 + 在屏1 + 解码中1，
// x264 常见 4+4=8)；无 VUI 退回 max_ref+reorder+3 的不相交最差账。
// 在屏 1 格是阻塞 commit 换来的(NONBLOCK 在飞翻页要押 2)。
// 按分辨率分档钳制：720 档帧大(1.4MB)钳 8 ≈11.3MB；360 档帧小(0.37MB)放到 16
// ≈5.9MB，容野素材的逆天 ref 数
#define VDEC_CAPTURE_BUF_MIN 5
#define VDEC_CAPTURE_BUF_MAX_LARGE 8
#define VDEC_CAPTURE_BUF_MAX_SMALL 16
#define VDEC_CAPTURE_LARGE_AREA (600 * 1000) /* 编码像素数阈值(720 档 942k) */

// ---------- 平滑上屏 buffer(VE spike 吸收) ----------
// 解码与定速解耦：解码线程出帧进 ring，pacer 线程按档期取出上屏。VE 一次
// spike(启动期 mv_col 懒分配撞 CMA 迁移可达 50-100ms，而正常 20-26ms 已贴
// 40Hz 档期的边)期间，pacer 照吃储备出帧，不再跟着一起停摆；ring 满即反压
// 解码线程，定速仍由 pacer 独家掌握。
// 每押一帧就多占一个 capture slot(360 档 0.37MB / 720 档 1.41MB)，直接叠加
// 在上面 VDEC_CAPTURE_BUF_MAX_* 之上(mediaplayer 的 cap_count += smooth)，
// 故按 MemTotal 分档：
//   32M(F1C100s, cma=16M) 预算已排满 → 0 = 不建 ring 不起 pacer，解码线程
//     自己睡档期上屏，与拆出 pacer 前逐字等价，一格都不多占
//   64M(F1C200s, cma=32M) 720 档 8+3 格 ≈15.5MB，留给 UI/overlay 仍宽裕
// 注意 ring 见底(pacer 日志的 ring=0/N)说明 VE 吞吐追不上素材帧率，那是素材
// 该降帧率，调大这里没用——储备只能吸收尖刺，填不平长期赤字。
#define MP_SMOOTH_BUFS_SMALL_MEM 0
#define MP_SMOOTH_BUFS_LARGE_MEM 8
// MemTotal 阈值(kB)：内核已扣掉自身保留，32M 机实报 ~26M、64M 机 ~58M
#define MP_MEM_LARGE_THRESHOLD_KB (40 * 1024)
// 上限兜底：ring + 解码账本不得撑爆 VDEC_MAX_CAP_BUFS(32)
#define MP_SMOOTH_BUFS_MAX 8

// ---------- SDROT 视频层 Y 翻转(倒装机型) ----------
// 存在此 DT key = 整机倒装,DEBE 扫描端 Y 倒扫,视频层内容需 app 用 VE SDROT
// 预翻(V4L2_CID_VFLIP)。不带 key 的机型此条链整段不启用,零拷贝路径原样不变。
// 见 docs/boe-flip-180.md、cedrus-rotate-usage.md。
#define SDROT_YFLIP_DT_PATH \
	"/proc/device-tree/soc/display-backend@1e60000/srgn,scanout-yflip"
// 翻转输出池的显示保持格数(入队未上屏1 + 在屏1)。启用翻转时,这几格从解码
// cap_count 对冲掉(解码 cap 不再承担显示保持,SDROT 一读完即放),净 CMA ≈ 不变。
#define SDROT_DISPLAY_HOLD 2


// ========== Animation Configuration ==========
#define LAYER_ANIMATION_STEP_TIME 20000 // 20ms, 1000ms / 50fps
#define OVERLAY_ANIMATION_STEP_TIME 33000 // 33ms, 1000ms / 30fps

// opinfo 元素引擎：单个 overlay 的元素数量上限（custom 超出时截断；
// arknights 预设也从这个池里分配，改小前先数一遍预设元素数）
#define OPINFO_ELEMENTS_MAX 24

#define OVERLAY_ANIMATION_OPINFO_ARKNIGHTS_DURATION (2000 * 1000) // 2s

// arknight overlay specfic. by frame count
#define OVERLAY_ANIMATION_OPINFO_NAME_START_FRAME 30
#define OVERLAY_ANIMATION_OPINFO_NAME_FRAME_PER_CODEPOINT 3
#define OVERLAY_ANIMATION_OPINFO_CODE_START_FRAME 40
#define OVERLAY_ANIMATION_OPINFO_CODE_FRAME_PER_CODEPOINT 3
#define OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_START_FRAME 40
#define OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_FRAME_PER_CODEPOINT 3
#define OVERLAY_ANIMATION_OPINFO_AUX_TEXT_START_FRAME 50
#define OVERLAY_ANIMATION_OPINFO_AUX_TEXT_FRAME_PER_CODEPOINT 2

#define OVERLAY_ANIMATION_OPINFO_COLOR_FADE_START_FRAME 15
#define OVERLAY_ANIMATION_OPINFO_COLOR_FADE_VALUE_PER_FRAME 10
#define OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE 192

#define OVERLAY_ANIMATION_OPINFO_BARCODE_START_FRAME 30
#define OVERLAY_ANIMATION_OPINFO_BARCODE_FRAME_PER_STATE 15

#define OVERLAY_ANIMATION_OPINFO_CLASSICON_START_FRAME 60
#define OVERLAY_ANIMATION_OPINFO_CLASSICON_FRAME_PER_STATE 15

#define OVERLAY_ANIMATION_OPINFO_LOGO_FADE_START_FRAME 30
#define OVERLAY_ANIMATION_OPINFO_LOGO_FADE_VALUE_PER_FRAME 5

#define OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_START_FRAME 100
#define OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT 40

#define OVERLAY_ANIMATION_OPINFO_LINE_UPPER_START_FRAME 80
#define OVERLAY_ANIMATION_OPINFO_LINE_LOWER_START_FRAME 90
#define OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT 40

#define OVERLAY_ANIMATION_OPINFO_ARROW_Y_INCR_PER_FRAME 1

#define OVERLAY_ANIMATION_OPINFO_DEFAULT_OPERATOR_NAME "OPERATOR"



// ========== UI Configuration ==========
#define UI_LAYER_ANIMATION_DURATION (500 * 1000) // 500ms

#define UI_IPC_HELPER_TIMER_TICK_PERIOD (100 * 1000)

// 统一切屏两段式(幕帘遮盖，见 screen_manager.c)：下潜到只露幕帘 → 藏着重绘 → 回升
#define UI_TRANSITION_DIP_DURATION  (200 * 1000) // 200ms
#define UI_TRANSITION_RISE_DURATION (300 * 1000) // 300ms
#define UI_TRANSITION_CURTAIN_RETRACT_MS 150
#define UI_WARNING_TIMER_TICK_PERIOD (200 * 1000)
#define UI_WARNING_DISPLAY_DURATION (3 * 1000 * 1000)
#define UI_WARNING_MAX_TITLE_LENGTH 64
#define UI_WARNING_MAX_DESC_LENGTH 128
#define UI_WARNING_MAX_ICON_LENGTH 16

#define UI_COLOR_ERROR 0xffb93030
#define UI_COLOR_WARNING 0xff8b7200
#define UI_COLOR_INFO 0xff646464 // 我不是故意的
#define UI_COLOR_OK 0xff0d6802

// ========== Battery Configuration ==========
// 数据来自 power_supply class(capacity 0-100 / status)，电压→电量的换算在
// dts 的 OCV 表里做，app 只看百分比。见 buildroot/board/rhodesisland/epass/BATTERY.md
#define UI_BATTERY_PSY_ROOT "/sys/class/power_supply"
#define UI_BATTERY_EMPTY_PERCENT 5
#define UI_BATTERY_1_4_PERCENT 25
#define UI_BATTERY_1_2_PERCENT 50
#define UI_BATTERY_3_4_PERCENT 75
#define UI_BATTERY_PADDING 10
#define UI_BATTERY_SIZE 30
#define UI_BATTERY_EMPTY_CHAR UI_ICON_BATTERY_EMPTY
#define UI_BATTERY_FULL_CHAR UI_ICON_BATTERY_FULL
#define UI_BATTERY_1_4_CHAR UI_ICON_BATTERY_QUARTER
#define UI_BATTERY_1_2_CHAR UI_ICON_BATTERY_HALF
#define UI_BATTERY_3_4_CHAR UI_ICON_BATTERY_THREE_QUARTERS
#define UI_BATTERY_CHARGING_CHAR UI_ICON_BOLT

// 低电压告警时间定值,单位tick
#define UI_BATTERY_LOW_BAT_WARNING_THRESHOLD 3
// 低电压跳闸时间定值,单位tick
#define UI_BATTERY_LOW_BAT_TRIP_THRESHOLD 6
#define UI_BATTERY_TIMER_TICK_PERIOD (10 * 1000 * 1000) // 10s


// ========== System Information Configuration ==========
#define SYSINFO_MEMINFO_PATH "/proc/meminfo"
#define SYSINFO_OSRELEASE_PATH "/etc/os-release"
#define SYSINFO_APP_PATH "/root/epass_drm_app"

// ========== Cached Assets Configuration ==========
#define CACHED_ASSETS_MAX_SIZE (VIDEO_HEIGHT * VIDEO_WIDTH * 3 / 2)
#if OVERLAY_USE_C8
// .c8 = tools/gen_c8.sh 离线产物(烘焙段索引位图, F-S 抖动)。抖动必须在目标
// 分辨率上做(先缩后抖), 所以按档各出一份, 编译期直接选文件。
#if UI_SCALE == 1
#define CACHEASSET_C8(name) name "_1x.c8"
#else
#define CACHEASSET_C8(name) name "_2x.c8"
#endif
#define CACHED_ASSETS_FILE_AK_BAR CACHEASSET_C8("ak_bar")
#define CACHED_ASSETS_FILE_BTM_LEFT_BAR CACHEASSET_C8("btm_left_bar")
#define CACHED_ASSETS_FILE_TOP_LEFT_RECT CACHEASSET_C8("top_left_rect")
#define CACHED_ASSETS_FILE_TOP_LEFT_RHODES CACHEASSET_C8("top_left_rhodes")
#define CACHED_ASSETS_FILE_TOP_RIGHT_BAR CACHEASSET_C8("top_right_bar")
#define CACHED_ASSETS_FILE_TOP_RIGHT_ARROW CACHEASSET_C8("top_right_arrow")
#else
// overlay 装饰图只存一份 2x(720 基准)素材, 走 stbi_load 直读 (无 lv_fs 盘符), 文件名
// 相对 res/, 运行时经 respath() 解析。1x 档由 cacheassets 加载时最近邻下采样到一半。
#define CACHED_ASSETS_FILE_AK_BAR "ak_bar.png"
#define CACHED_ASSETS_FILE_BTM_LEFT_BAR "btm_left_bar.png"
#define CACHED_ASSETS_FILE_TOP_LEFT_RECT "top_left_rect.png"
#define CACHED_ASSETS_FILE_TOP_LEFT_RHODES "top_left_rhodes.png"
#define CACHED_ASSETS_FILE_TOP_RIGHT_BAR "top_right_bar.png"
#define CACHED_ASSETS_FILE_TOP_RIGHT_ARROW "top_right_arrow.png"
#endif


// ========== Exitcode Definition ==========
#define EXITCODE_NORMAL 0
#define EXITCODE_RESTART_APP 1
#define EXITCODE_APPSTART 2
#define EXITCODE_SHUTDOWN 3
#define EXITCODE_FORMAT_SD_CARD 4
#define EXITCODE_SRGN_CONFIG 5


// ========== Display Image Configuration ==========
#define DISPLAYIMG_MAX_COUNT 128
#define DISPLAYIMG_MAX_PATH_LENGTH 128
#define DISPLAYIMG_PATH "/dispimg/"


// ========== PC Target 路径覆写 ==========
// PC Target（SDL 后端 + ffmpeg 解码）：设备绝对路径整体挪到可写的数据目录，
// 其余 sysfs/dev 路径保持原样（open 失败即天然 stub：电池/背光/SD 检测）。
// 数据目录布局与设备根文件系统同构：pcdata/{assets,app,dispimg,epass_cfg.bin,*.log}
#ifdef EPASS_PC_TARGET
#ifndef EPASS_PC_DATA_DIR
#define EPASS_PC_DATA_DIR "./pcdata"
#endif
#undef SETTINGS_FILE_PATH
#define SETTINGS_FILE_PATH EPASS_PC_DATA_DIR "/epass_cfg.bin"
#undef FILEMANAGER_LAST_DIR_FILE
#define FILEMANAGER_LAST_DIR_FILE EPASS_PC_DATA_DIR "/epass_fm_lastdir.txt"
#undef PRTS_OPERATOR_PARSE_LOG
#define PRTS_OPERATOR_PARSE_LOG EPASS_PC_DATA_DIR "/asset.log"
#undef PRTS_ORDER_FILE
#define PRTS_ORDER_FILE EPASS_PC_DATA_DIR "/epass_oporder.txt"
#undef PRTS_ASSET_DIR
#define PRTS_ASSET_DIR EPASS_PC_DATA_DIR "/assets/"
#undef APPS_PARSE_LOG
#define APPS_PARSE_LOG EPASS_PC_DATA_DIR "/apps.log"
#undef APPS_DIR
#define APPS_DIR EPASS_PC_DATA_DIR "/app/"
#undef SYSINFO_APP_PATH
#define SYSINFO_APP_PATH "/proc/self/exe"
#undef DISPLAYIMG_PATH
#define DISPLAYIMG_PATH EPASS_PC_DATA_DIR "/dispimg/"
#endif // EPASS_PC_TARGET
