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
# Cedarx 多媒体库配置 (Allwinner 专用, 可选)
# ============================================================================
# 可通过以下方式配置:
# -DENABLE_CEDARX=OFF 禁用 Cedarx 支持

option(ENABLE_CEDARX "Enable Cedarx video decoding support" ON)


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

  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c
  src/render/fbdraw.c

  src/ui/font_registry.c
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
)

# 如果 Cedarx 可用，添加 mediaplayer
if(ENABLE_CEDARX)
  list(APPEND EPASS_SOURCES src/render/mediaplayer.c)
endif()

add_executable(epass_drm_app ${EPASS_SOURCES})

# ============================================================================
# 包含目录
# ============================================================================
target_include_directories(epass_drm_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens/screens
  ${LIBDRM_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIR}
  ${FREETYPE_INCLUDE_DIRS}
)


# ============================================================================
# 编译定义
# ============================================================================
string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")

# 屏目标由构建注入 (覆盖 config.h 的兜底默认)
set(EPASS_SCREEN "360x640" CACHE STRING "Target screen: 360x640 | 480x854 | 720x1280")
if(EPASS_SCREEN STREQUAL "360x640")
  set(EPASS_SCREEN_DEFINE USE_360_640_SCREEN)
elseif(EPASS_SCREEN STREQUAL "480x854")
  set(EPASS_SCREEN_DEFINE USE_480_854_SCREEN)
elseif(EPASS_SCREEN STREQUAL "720x1280")
  set(EPASS_SCREEN_DEFINE USE_720_1280_SCREEN)
else()
  message(FATAL_ERROR "Invalid EPASS_SCREEN: ${EPASS_SCREEN} (use 360x640 | 480x854 | 720x1280)")
endif()
message(STATUS "Screen target: ${EPASS_SCREEN} -> ${EPASS_SCREEN_DEFINE}")

target_compile_definitions(epass_drm_app PRIVATE
  _DEFAULT_SOURCE
  ${EPASS_SCREEN_DEFINE}
  EPASS_GIT_VERSION="${GIT_COMMIT}"
  EPASS_GIT_TAG="${GIT_TAG}"
  COMPILE_TIME="${COMPILE_TIME}"
)

# 如果 Cedarx 可用，定义宏
if(ENABLE_CEDARX)
  target_compile_definitions(epass_drm_app PRIVATE HAVE_CEDARX=1)
else()
  target_compile_definitions(epass_drm_app PRIVATE HAVE_CEDARX=0)
endif()

# ============================================================================
# 编译选项
# ============================================================================
target_compile_options(epass_drm_app PRIVATE
  -Wall -Wextra -Wno-unused-parameter
  -Wno-format-truncation
  -ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/=/
  ${LIBEVDEV_CFLAGS_OTHER}
  $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
)

target_compile_options(lvgl PRIVATE
  $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
)


# ============================================================================
# 链接库
# ============================================================================
target_link_libraries(epass_drm_app
  # 数学库和系统库
  m
  dl
  pthread
  rt

  # 外部依赖
  ${LIBEVDEV_LIBRARIES}
  ${LIBDRM_LIBRARIES}
  ${PNG_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  lvgl
)

# 如果 Cedarx 可用，链接 Cedarx 库
if(ENABLE_CEDARX)
  target_link_libraries(epass_drm_app
    MemAdapter
    vdecoder
    VE
    videoengine
    cdc_base
    cdx_parser
    cdx_stream
    cdx_base
    cdx_common
    # libcdx_stream 和 libcdx_parser 依赖 OpenSSL
    ssl
    crypto
  )
endif()

# ============================================================================
# 资源拷贝：按 config.h 的 res/ 结构落到可执行文件同级
#   assets/*.png   -> <exe>/res/
#   font/*.otf     -> <exe>/res/fonts/      (子集化产物，见 tools/font_generate)
#   assets/fallback/ -> <exe>/res/fallback/
# 运行时由 respath 模块按 /proc/self/exe 解析这个目录。
# 字体取 font/ 根下的裁剪版 (原件在 font/original/ 本地，不参与构建)。
# ============================================================================
set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(FONT_DIR   ${CMAKE_SOURCE_DIR}/font)
set(RES_OUT_DIR $<TARGET_FILE_DIR:epass_drm_app>/res)
file(GLOB RES_PNG  CONFIGURE_DEPENDS ${ASSETS_DIR}/*.png)
file(GLOB RES_FONT CONFIGURE_DEPENDS ${FONT_DIR}/*.otf)

add_custom_command(TARGET epass_drm_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}/fonts
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_PNG}  ${RES_OUT_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_FONT} ${RES_OUT_DIR}/fonts/
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/fallback ${RES_OUT_DIR}/fallback
  COMMENT "Copying assets -> res/ next to executable"
  VERBATIM
)

# ============================================================================
# 安装规则 (可选)：bin/epass_drm_app 与 bin/res/ 同级，保持运行时相对结构
# ============================================================================
install(TARGETS epass_drm_app DESTINATION bin)
install(DIRECTORY ${ASSETS_DIR}/ DESTINATION bin/res
        FILES_MATCHING PATTERN "*.png")
install(FILES ${RES_FONT} DESTINATION bin/res/fonts)
install(DIRECTORY ${ASSETS_DIR}/fallback DESTINATION bin/res)
