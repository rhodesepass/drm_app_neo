#pragma once

// ========== Application Information ==========
#define APP_SUBCODENAME "proj0cpy"
#define APP_BARNER \
    "           =-+           \n" \
    "     +@@@@@  @@@@@@        Rhodes Island\n" \
    "     +@*.:@@ @@  %@-       Electrnic Pass\n" \
    "     *@@@%:  @@@@@=:       DRM System\n" \
    "   ==+@      @@  @@ .=   \n" \
    "  +                   *    CodeName:\n" \
    "   ==%@@@@@@ %@@@@@ .+     " APP_SUBCODENAME "\n" \
    "     :  @=   %@%.  -       \n" \
    "       :@=      +@@        Rhodes Island\n" \
    "        @#   %@@@@@        Engineering Dept.\n" \
    "           =:=        \n"

// ========== Settings Configuration ==========
#define SETTINGS_FILE_PATH "/root/epass_cfg.bin"
#define SETTINGS_MAGIC 0x45504153434F4E46
#define SETTINGS_VERSION 1

// ========== PRTS Timer Configuration ==========
#define PRTS_TIMER_MAX 1024

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

    #define UI_OPLIST_Y 200
    
    // UI-信息Overlay叠层 左上角的矩形偏移量
    #define OVERLAY_ARKNIGHTS_RECT_OFFSET_X 60

    // UI-信息Overlay叠层 左下角“- Arknights -”矩形文字 偏移量
    #define OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_X 120
    #define OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y 120
    
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

// ========== Asset Configuration ==========
#define UI_OVERLAY_ARKNIGHTS_PREFIX "A:/root/res/"

// ========== Layer Animation Configuration ==========
#define LAYER_ANIMATION_STEP_TIME 20000 // 20ms, 1000ms / 50fps

// ========== UI Configuration ==========
#define UI_LAYER_ANIMATION_DURATION (500 * 1000) // 500ms