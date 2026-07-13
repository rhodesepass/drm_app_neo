cmake_minimum_required(VERSION 3.10)
project(epass_ui_sim C)

# 仓库根目录 (sim 的上一级)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 模拟器字体目录: 优先仓库内 font/ (子集, 现状不变); 若已移除, 回退到兄弟仓
# epass-fonts/original (共享母本, 全量)。设备侧字体走 epass-fonts 包 (见顶层 CMakeLists)。
if(EXISTS ${REPO}/font/SourceHanSansSC-Regular.otf)
  set(SIM_FONT_DIR ${REPO}/font)
elseif(EXISTS ${REPO}/../epass-fonts/original/SourceHanSansSC-Regular.otf)
  set(SIM_FONT_DIR ${REPO}/../epass-fonts/original)
else()
  message(FATAL_ERROR "找不到模拟器字体: 既无 ${REPO}/font 也无 ../epass-fonts/original")
endif()
message(STATUS "sim font dir: ${SIM_FONT_DIR}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 依赖
# ============================================================================
find_package(PkgConfig REQUIRED)
find_package(PNG REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FREETYPE REQUIRED freetype2)
# overlay 仿真编译真实的 overlay/*.c，drm_warpper.h 引 xf86drm.h —— 只要头文件，不链接
pkg_check_modules(LIBDRM REQUIRED libdrm)

# overlay 仿真的固定视频底图（编译期烘进二进制，-DSIM_VIDEO_IMG= 可换）
if(NOT SIM_VIDEO_IMG)
  set(SIM_VIDEO_IMG ${REPO}/assets/fallback/pr_glitch.jpg)
endif()
message(STATUS "overlay sim video image: ${SIM_VIDEO_IMG}")

# ============================================================================
# LVGL (用 sim 专用的 lv_conf.h: 开 SDL + FreeType)
# ============================================================================
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
add_subdirectory(${REPO}/lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

# ============================================================================
# 模拟器可执行 —— 一次产出两档分辨率 (360x640 / 720x1280)，便于并排对比。
# 源码与 liblvgl 共用，仅屏宏 (USE_*_SCREEN) 不同 → 每档一个 target。
# ============================================================================
set(SIM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_backend.c
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_plane.c
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_sdl_window.c

  ${REPO}/src/ui_screens/screen_manager.c
  ${REPO}/src/ui_screens/styles.c
  ${REPO}/src/ui_screens/screens/screen_common.c
  ${REPO}/src/ui_screens/screens/screen_mainmenu.c
  ${REPO}/src/ui_screens/screens/screen_oplist.c
  ${REPO}/src/ui_screens/screens/screen_sysinfo.c
  ${REPO}/src/ui_screens/screens/screen_spinner.c
  ${REPO}/src/ui_screens/screens/screen_displayimg.c
  ${REPO}/src/ui_screens/screens/screen_filemanager.c
  ${REPO}/src/ui_screens/screens/screen_settings.c
  ${REPO}/src/ui_screens/screens/screen_applist.c
  ${REPO}/src/ui_screens/screens/screen_warning.c
  ${REPO}/src/ui_screens/screens/screen_confirm.c
  ${REPO}/src/ui_screens/screens/screen_usbselect.c
  ${REPO}/src/ui/uix_session.c
  ${REPO}/src/ui/font_registry.c
  ${REPO}/src/ui/ui_theme.c
  ${REPO}/src/utils/log.c
)

# name: 可执行名; screen_define: 屏宏 (覆盖 config.h 的兜底默认)
function(add_sim_target name screen_define)
  add_executable(${name} ${SIM_SOURCES})
  target_include_directories(${name} PRIVATE
    ${REPO}
    ${REPO}/lvgl
    ${REPO}/src
    ${REPO}/src/ui_screens
    ${REPO}/src/ui_screens/screens
    ${SDL2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
  )
  # 字体目录: FreeType 直接读, 不走 lv_fs, 无盘符前缀
  # 图片目录: 走 lv_fs (libpng 解码), 需带盘符 'A:' 前缀
  target_compile_definitions(${name} PRIVATE
    ${screen_define}
    FONT_REGISTRY_DIR="${SIM_FONT_DIR}"
    UI_IMG_DIR="A:${SIM_FONT_DIR}"
  )
  target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(${name}
    lvgl
    ${SDL2_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${PNG_LIBRARIES}
    m
    pthread
  )
endfunction()

add_sim_target(epass_ui_sim_360x640  USE_360_640_SCREEN)
add_sim_target(epass_ui_sim_720x1280 USE_720_1280_SCREEN)

# ============================================================================
# Overlay 仿真 —— 真实 overlay 代码（元素引擎/transitions/worker/prts_timer/
# layer_animation）跑在 SDL 合成窗口上；drm_warpper 用 sim_drm_warpper.c mock，
# video 层为编译期固定图片。用法见 docs/overlay_dev_note.md 的 "PC 仿真" 一节。
# ============================================================================
set(OVERLAY_SIM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/overlay_sim_main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/overlay_sim_presets.c
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_drm_warpper.c

  ${REPO}/src/overlay/overlay.c
  ${REPO}/src/overlay/opinfo.c
  ${REPO}/src/overlay/transitions.c
  ${REPO}/src/render/fbdraw.c
  ${REPO}/src/render/layer_animation.c
  ${REPO}/src/ui/font_registry.c
  ${REPO}/src/utils/timer.c
  ${REPO}/src/utils/cacheassets.c
  ${REPO}/src/utils/imgscale.c
  ${REPO}/src/utils/misc.c
  ${REPO}/src/utils/cJSON.c
  ${REPO}/src/utils/code128.c
  ${REPO}/src/utils/respath.c
  ${REPO}/src/utils/log.c
)

function(add_overlay_sim_target name screen_define)
  add_executable(${name} ${OVERLAY_SIM_SOURCES})
  target_include_directories(${name} PRIVATE
    ${REPO}
    ${REPO}/lvgl
    ${REPO}/src
    ${SDL2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
  )
  target_compile_definitions(${name} PRIVATE
    _DEFAULT_SOURCE
    ${screen_define}
    FONT_REGISTRY_DIR="${SIM_FONT_DIR}"
    SIM_VIDEO_IMG="${SIM_VIDEO_IMG}"
  )
  target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(${name}
    lvgl
    ${SDL2_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${PNG_LIBRARIES}
    m
    pthread
    rt
  )
  # cacheassets/预设素材走可执行文件同级 res/（respath），照抄设备 target 的拷贝
  set(RES_OUT_DIR $<TARGET_FILE_DIR:${name}>/res)
  file(GLOB SIM_RES_PNG CONFIGURE_DEPENDS ${REPO}/assets/*.png)
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SIM_RES_PNG} ${RES_OUT_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${REPO}/assets/fallback ${RES_OUT_DIR}/fallback
    COMMENT "Copying assets -> res/ next to ${name}"
    VERBATIM
  )
endfunction()

add_overlay_sim_target(epass_overlay_sim_360x640  USE_360_640_SCREEN)
add_overlay_sim_target(epass_overlay_sim_720x1280 USE_720_1280_SCREEN)
