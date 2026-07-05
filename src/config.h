#pragma once
#include "icons.h"

// ========== Resource Layout ==========
// 内置资源放在可执行文件同级的 RES_SUBDIR 子目录, 运行时由 respath 模块解析。
// CMake 按本结构把仓库 assets/ 拷过去: 根 *.png, fonts/*.otf, fallback/。
#define RES_SUBDIR "res"
#define RES_FONTS_SUBDIR "fonts"
#define RES_DEFAULT_ICON_FILE "defaulticon.png"

// ========== Application Information ==========
#define APP_SUBCODENAME "proj0cpy"
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
    "本设计方案按“原样”提供，不附带任何形式的明示或默示担保。白银不对使用本设计方案造成的任何索赔、损害或其他责任承担责任。" \
    "白银不参与本项目的任何商业活动，也不从中获取任何利益，亦无义务对本设计方案进行任何形式的维护或更新。"

#define APP_VERSION EPASS_GIT_TAG
#define APP_VERSION_STRING (APP_VERSION "_" EPASS_GIT_VERSION)

// ========== Settings Configuration ==========
#define SETTINGS_FILE_PATH "/root/epass_cfg.bin"
#define SETTINGS_MAGIC 0x45504153434F4E46
#define SETTINGS_VERSION 4
#define SETTINGS_BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"

// ========== Storage Configuration ==========
#define NAND_MOUNT_POINT "/"
#define SD_MOUNT_POINT "/sd"
#define SD_DEV_PATH "/dev/mmcblk0"

// ========== PRTS Configuration ==========
#define PRTS_OPERATORS_MAX 256
#define PRTS_TIMER_MAX 1024
#define PRTS_OPERATOR_PARSE_LOG "/root/asset.log"
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
#define APPS_CONFIG_VERSION 1
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
#define OVERLAY_ARKNIGHTS_RECT_OFFSET_X     (60 * UI_SCALE)
// 下方信息区 左偏移
#define OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X (70 * UI_SCALE)
// 干员名 Y 偏移
#define OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y   (415 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y (455 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y (475 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_LINE_WIDTH        (280 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y   (459 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y (480 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y (525 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_WIDTH  (50 * UI_SCALE)
#define OVERLAY_ARKNIGHTS_CLASS_ICON_HEIGHT (50 * UI_SCALE)
// 左下角 "- Arknights -" 矩形文字 Y 偏移
#define OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y   (578 * UI_SCALE)
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


// ========== Animation Configuration ==========
#define LAYER_ANIMATION_STEP_TIME 20000 // 20ms, 1000ms / 50fps
#define OVERLAY_ANIMATION_STEP_TIME 33000 // 33ms, 1000ms / 30fps

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
#define UI_BATTERY_ADC_PATH "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"
#define UI_BATTERY_EMPTY_VALUE 2140
#define UI_BATTERY_1_4_VALUE 2230
#define UI_BATTERY_1_2_VALUE 2320
#define UI_BATTERY_3_4_VALUE 2410
#define UI_BATTERY_FULL_VALUE 2500
#define UI_BATTERY_CHARGING_VALUE 2600
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
// overlay 装饰图只存一份 2x(720 基准)素材, 走 stbi_load 直读 (无 lv_fs 盘符), 文件名
// 相对 res/, 运行时经 respath() 解析。1x 档由 cacheassets 加载时最近邻下采样到一半。
#define CACHED_ASSETS_FILE_AK_BAR "ak_bar.png"
#define CACHED_ASSETS_FILE_BTM_LEFT_BAR "btm_left_bar.png"
#define CACHED_ASSETS_FILE_TOP_LEFT_RECT "top_left_rect.png"
#define CACHED_ASSETS_FILE_TOP_LEFT_RHODES "top_left_rhodes.png"
#define CACHED_ASSETS_FILE_TOP_RIGHT_BAR "top_right_bar.png"
#define CACHED_ASSETS_FILE_TOP_RIGHT_ARROW "top_right_arrow.png"


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
