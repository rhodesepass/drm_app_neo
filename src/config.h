#pragma once

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

#define APP_RELEASE

// ========== Settings Configuration ==========
#define SETTINGS_FILE_PATH "/root/epass_cfg.bin"
#define SETTINGS_MAGIC 0x45504153434F4E46
#define SETTINGS_VERSION 2
#define SETTINGS_BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"

// ========== Storage Configuration ==========
#define NAND_MOUNT_POINT "/"
#define SD_MOUNT_POINT "/sd"
#define SD_DEV_PATH "/dev/mmcblk0"

// ========== PRTS Configuration ==========
#define PRTS_OPERATORS_MAX 128
#define PRTS_TIMER_MAX 1024
#define PRTS_OPERATOR_PARSE_LOG "/root/asset.log"
#define PRTS_ASSET_VERSION_NUMBER 1
#define PRTS_DEFAULT_ICON_PATH "A:/root/res/defaulticon.png"
#define PRTS_ASSET_DIR "/assets/"
#define PRTS_ASSET_DIR_SD SD_MOUNT_POINT "/assets/"
#define PRTS_TICK_PERIOD (1000 * 1000)
#define PRTS_FALLBACK_ASSET_DIR "/root/res/fallback/"

// ========== Apps Configuration ==========
#define APPS_MAX_COUNT          64
#define APPS_CONFIG_FILENAME    "appconfig.json"
#define APPS_CONFIG_VERSION     1
#define APPS_DIR_NAND           "/apps/"
#define APPS_DIR_SD             SD_MOUNT_POINT "/apps/"
#define APPS_PARSE_LOG_PATH     "/root/apps.log"
#define APPS_DEFAULT_ICON_PATH  "A:/root/res/default_app_icon.png"
#define APP_ICON_SIZE           48

// ========== Screen Configuration ==========
#define USE_360_640_SCREEN
// #define USE_480_854_SCREEN
// #define USE_720_1280_SCREEN

// resolution alternatives.
#if defined(USE_360_640_SCREEN)
    #define VIDEO_WIDTH 384
    #define VIDEO_HEIGHT 640
    #define UI_WIDTH 360
    #define UI_HEIGHT 640
    #define OVERLAY_WIDTH 360
    #define OVERLAY_HEIGHT 640
    #define SCREEN_WIDTH 360
    #define SCREEN_HEIGHT 640

    // 干员列表和亮度设置的Y坐标
    #define UI_OPLIST_Y 250
    #define UI_SPINNER_INTRO_Y 580
    #define UI_MAINMENU_Y 190
    #define UI_WARNING_Y 565
    #define UI_CONFIRM_Y (UI_HEIGHT - 125)



    
    // UI-信息Overlay叠层 左上角的矩形偏移量
    #define OVERLAY_ARKNIGHTS_RECT_OFFSET_X 60

    // UI-信息Overlay叠层 下方信息区域 左偏移量
    #define OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X 70

    
    // UI-信息Overlay叠层 下方信息区域 干员名 偏移量
    #define OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y 415

    #define OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y 455
    #define OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y 475
    #define OVERLAY_ARKNIGHTS_LINE_WIDTH 280

    #define OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y 457
    #define OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y 480

    #define OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y 525
    #define OVERLAY_ARKNIGHTS_CLASS_ICON_WIDTH 50
    #define OVERLAY_ARKNIGHTS_CLASS_ICON_HEIGHT 50
    // UI-信息Overlay叠层 左下角“- Arknights -”矩形文字 偏移量
    #define OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y 578
    // UI-信息Overlay叠层 下方信息区域 辅助文字 偏移量
    #define OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y 592
    #define OVERLAY_ARKNIGHTS_AUX_TEXT_LINE_HEIGHT 15

    // UI-信息Overlay叠层 左下角 条码 偏移量
    #define OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y 450
    #define OVERLAY_ARKNIGHTS_BARCODE_WIDTH 50
    #define OVERLAY_ARKNIGHTS_BARCODE_HEIGHT 180

    // UI-信息Overlay叠层 右上角 装饰箭头 偏移量
    #define OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y 100


    
    
#elif defined(USE_480_854_SCREEN)
    #error "USE_480_854_SCREEN is not supported yet!"
#elif defined(USE_720_1280_SCREEN)
    #error "USE_720_1280_SCREEN is not supported yet!"
#endif // USE_360_640_SCREEN, USE_480_854_SCREEN, USE_720_1280_SCREEN

// ========== DRM Warpper Layer Configuration ==========
#define DRM_WARPPER_LAYER_UI 2
#define DRM_WARPPER_LAYER_OVERLAY 1
#define DRM_WARPPER_LAYER_VIDEO 0

// ========== Media Player Configuration ==========
#define VBVBUFFERSIZE 2 * 1024 * 1024
#define BUF_CNT_4_DI 1
#define BUF_CNT_4_LIST 1
#define BUF_CNT_4_ROTATE 0
#define BUF_CNT_4_SMOOTH 1


// ========== Animation Configuration ==========
#define LAYER_ANIMATION_STEP_TIME 20000 // 20ms, 1000ms / 50fps
#define OVERLAY_ANIMATION_STEP_TIME 20000 // 33ms, 1000ms / 30fps

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

#define UI_LAYER_ANIMATION_INTRO_SPINNER_TRANSITION_DURATION (200 * 1000) // 200ms
#define UI_LAYER_ANIMATION_INTRO_LOADSCREEN_DELAY (500 * 1000)
#define UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DURATION (300 * 1000) // 300ms
#define UI_LAYER_ANIMATION_INTRO_DEST_TRANSITION_DELAY (500 * 1000) // 500ms
#define UI_WARNING_TIMER_TICK_PERIOD (200 * 1000)
#define UI_WARNING_DISPLAY_DURATION (3 * 1000 * 1000)

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
#define UI_BATTERY_EMPTY_CHAR "\uf244"
#define UI_BATTERY_FULL_CHAR "\uf240"
#define UI_BATTERY_1_4_CHAR "\uf243"
#define UI_BATTERY_1_2_CHAR "\uf242"
#define UI_BATTERY_3_4_CHAR "\uf241"
#define UI_BATTERY_CHARGING_CHAR "\uf0e7"

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
#define CACHED_ASSETS_ASSET_PATH "/root/res/"
#define CACHED_ASSETS_ASSET_PATH_AK_BAR CACHED_ASSETS_ASSET_PATH "ak_bar.png"
#define CACHED_ASSETS_ASSET_PATH_BTM_LEFT_BAR CACHED_ASSETS_ASSET_PATH "btm_left_bar.png"
#define CACHED_ASSETS_ASSET_PATH_TOP_LEFT_RECT CACHED_ASSETS_ASSET_PATH "top_left_rect.png"
#define CACHED_ASSETS_ASSET_PATH_TOP_LEFT_RHODES CACHED_ASSETS_ASSET_PATH "top_left_rhodes.png"
#define CACHED_ASSETS_ASSET_PATH_TOP_RIGHT_BAR CACHED_ASSETS_ASSET_PATH "top_right_bar.png"
#define CACHED_ASSETS_ASSET_PATH_TOP_RIGHT_ARROW CACHED_ASSETS_ASSET_PATH "top_right_arrow.png"


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
#define DISPLAYIMG_PATH "/root/dispimg/"