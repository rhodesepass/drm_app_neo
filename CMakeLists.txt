cmake_minimum_required(VERSION 3.10)
project(epass_drm_app)
include(FetchContent)

# ============================================================================
# Git 版本信息
# ============================================================================
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
  )
  execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_COMMIT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
  )
else()
  set(GIT_TAG "unknown")
  set(GIT_COMMIT "unknown")
endif()
message(STATUS "Git tag: ${GIT_TAG}")
message(STATUS "Git commit: ${GIT_COMMIT}")

# ============================================================================
# 编译选项
# ============================================================================
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set(APP_RELEASE ON CACHE BOOL "Build type" FORCE)
  message(STATUS "No build type specified, defaulting to release mode.")
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(APP_RELEASE OFF CACHE BOOL "Build type" FORCE)
    message(STATUS "App is building in debug mode.")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(APP_RELEASE ON CACHE BOOL "Build type" FORCE)
    message(STATUS "App is building in release mode.")
  else()
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
  endif()
endif()

# ============================================================================
# 依赖库查找
# ============================================================================
find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(LIBDRM REQUIRED libdrm)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# ============================================================================
# LVGL 配置
# ============================================================================
# 设置为 CACHE 变量以覆盖 LVGL 的默认值
set(LV_BUILD_CONF_PATH ${CMAKE_SOURCE_DIR}/src/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build LVGL demos" FORCE)
add_subdirectory(lvgl)
# LVGL 开启 LV_USE_FREETYPE 后，其适配源需要 freetype 头
target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS})

# ============================================================================
# 源文件
# ============================================================================
set(EPASS_SOURCES
  src/main.c

  src/prts/prts.c
  src/prts/operators.c

  src/apps/apps.c
  src/apps/extmap.c
  src/apps/apps_cfg_parse.c
  src/apps/ipc_common.c
  src/apps/ipc_server.c
  src/apps/ipc_handler.c

  src/driver/drm_warpper.c
  src/driver/key_enc_evdev.c

  src/render/mediaplayer.c
  src/vdec/mp4_demux.c
  src/vdec/bitreader.c
  src/vdec/nalu.c
  src/vdec/h264_parser.c
  src/vdec/h264_dpb.c
  src/vdec/vdec_v4l2.c

  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c
  src/render/fbdraw.c

  src/ui/font_registry.c
  src/ui/ui_theme.c
  src/ui/filemanager.c
  src/ui/battery.c
  src/ui/ipc_helper.c

  # 手写 UI (取代 EEZ)
  src/ui_screens/screen_manager.c
  src/ui_screens/styles.c
  src/ui_screens/screens/screen_common.c
  src/ui_screens/screens/screen_mainmenu.c
  src/ui_screens/screens/screen_oplist.c
  src/ui_screens/screens/screen_sysinfo.c
  src/ui_screens/screens/screen_spinner.c
  src/ui_screens/screens/screen_displayimg.c
  src/ui_screens/screens/screen_filemanager.c
  src/ui_screens/screens/screen_settings.c
  src/ui_screens/screens/screen_applist.c
  src/ui_screens/screens/screen_warning.c
  src/ui_screens/screens/screen_confirm.c
  src/ui_screens/platform/ui_backend_device.c
  src/ui_screens/platform/ui_plane_device.c
  src/ui_screens/platform/ui_services_device.c

  src/overlay/overlay.c
  src/overlay/transitions.c
  src/overlay/opinfo.c

  src/utils/spsc_queue.c
  src/utils/log.c
  src/utils/timer.c
  src/utils/settings.c
  src/utils/cacheassets.c
  src/utils/respath.c
  src/utils/code128.c
  src/utils/cJSON.c
  src/utils/uuid.c
  src/utils/misc.c
  src/utils/imgscale.c
)

string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")

set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(FONT_DIR   ${CMAKE_SOURCE_DIR}/font)
file(GLOB RES_PNG  CONFIGURE_DEPENDS ${ASSETS_DIR}/*.png)
file(GLOB RES_FONT CONFIGURE_DEPENDS ${FONT_DIR}/*.otf)

# ============================================================================
# 可执行 —— 一次产出两档分辨率 (360x640 / 720x1280)，源码共用，仅屏宏不同。
# name: 可执行名; screen_define: 屏宏 (覆盖 config.h 的兜底默认)
# ============================================================================
function(add_epass_target name screen_define)
  add_executable(${name} ${EPASS_SOURCES})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens/screens
    ${LIBDRM_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
  )
  target_compile_definitions(${name} PRIVATE
    _DEFAULT_SOURCE
    ${screen_define}
    EPASS_GIT_VERSION="${GIT_COMMIT}"
    EPASS_GIT_TAG="${GIT_TAG}"
    COMPILE_TIME="${COMPILE_TIME}"
  )
  target_compile_options(${name} PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -Wno-format-truncation
    -ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/=/
    ${LIBEVDEV_CFLAGS_OTHER}
    $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
  )
  target_link_libraries(${name}
    m
    dl
    pthread
    rt
    ${LIBEVDEV_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    ${PNG_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    lvgl
  )
  # 资源拷贝：按 config.h 的 res/ 结构落到可执行文件同级
  set(RES_OUT_DIR $<TARGET_FILE_DIR:${name}>/res)
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_PNG}  ${RES_OUT_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_FONT} ${RES_OUT_DIR}/fonts/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/fallback ${RES_OUT_DIR}/fallback
    COMMENT "Copying assets -> res/ next to ${name}"
    VERBATIM
  )
endfunction()

target_compile_options(lvgl PRIVATE
  $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
)

# vdec 解码栈独立验证工具（真机 bring-up 用，make vdec_selftest 手动构建）
add_executable(vdec_selftest EXCLUDE_FROM_ALL
  tools/vdec_selftest.c
  src/vdec/mp4_demux.c
  src/vdec/bitreader.c
  src/vdec/nalu.c
  src/vdec/h264_parser.c
  src/vdec/h264_dpb.c
  src/vdec/vdec_v4l2.c
)
target_include_directories(vdec_selftest PRIVATE src/vdec ${LIBDRM_INCLUDE_DIRS})
target_link_libraries(vdec_selftest ${LIBDRM_LIBRARIES})

add_epass_target(app_360 USE_360_640_SCREEN)
add_epass_target(app_720 USE_720_1280_SCREEN)

# ============================================================================
# 安装规则 (可选)：bin/app_* 与 bin/res/ 同级，保持运行时相对结构
# ============================================================================
install(TARGETS app_360 app_720 DESTINATION bin)
install(DIRECTORY ${ASSETS_DIR}/ DESTINATION bin/res
        FILES_MATCHING PATTERN "*.png")
install(FILES ${RES_FONT} DESTINATION bin/res/fonts)
install(DIRECTORY ${ASSETS_DIR}/fallback DESTINATION bin/res)
