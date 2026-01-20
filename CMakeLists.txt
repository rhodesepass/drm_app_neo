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
endif()

# ============================================================================
# Cedarx 多媒体库配置 (Allwinner 专用, 可选)
# ============================================================================
# 可通过以下方式配置:
#   1. 环境变量: CEDARX_ROOT, CEDARX_LIB_PATH
#   2. CMake 变量: -DCEDARX_ROOT=... -DCEDARX_LIB_PATH=...
#   3. -DENABLE_CEDARX=OFF 禁用 Cedarx 支持

option(ENABLE_CEDARX "Enable Cedarx video decoding support" ON)


# ============================================================================
# 依赖库查找
# ============================================================================
find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(LIBDRM REQUIRED libdrm)

# ============================================================================
# LVGL 配置
# ============================================================================
# 设置为 CACHE 变量以覆盖 LVGL 的默认值
set(LV_BUILD_CONF_PATH ${CMAKE_SOURCE_DIR}/src/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build LVGL demos" FORCE)
add_subdirectory(lvgl)

# ============================================================================
# 源文件
# ============================================================================
FILE(GLOB UI_SOURCES "eez_design/src/ui/*.c")

set(EPASS_SOURCES
  src/main.c

  src/prts/prts.c
  src/prts/operators.c

  src/driver/drm_warpper.c
  src/driver/key_enc_evdev.c

  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c
  src/render/fbdraw.c

  ${UI_SOURCES}
  src/ui/filemanager.c
  src/ui/scr_transition.c
  src/ui/battery.c
  src/ui/actions_warning.c
  src/ui/actions_mainscreen.c
  src/ui/actions_settings.c
  src/ui/actions_displayimg.c
  src/ui/actions_sysinfo.c
  src/ui/actions_oplist.c
  src/ui/actions_confirm.c

  src/overlay/overlay.c
  src/overlay/transitions.c
  src/overlay/opinfo.c

  src/utils/spsc_queue.c
  src/utils/log.c
  src/utils/timer.c
  src/utils/settings.c
  src/utils/cacheassets.c
  src/utils/code128.c
  src/utils/cJSON.c
  src/utils/uuid.c
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
  ${CMAKE_CURRENT_SOURCE_DIR}/eez_design/src/ui
  ${LIBDRM_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIR}
)


# ============================================================================
# 编译定义
# ============================================================================
string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")

target_compile_definitions(epass_drm_app PRIVATE
  _DEFAULT_SOURCE
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
# 安装规则 (可选)
# ============================================================================
install(TARGETS epass_drm_app DESTINATION bin)
