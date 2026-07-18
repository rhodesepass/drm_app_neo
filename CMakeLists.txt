cmake_minimum_required(VERSION 3.10)
project(epass_drm_app C)
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

# TARGET_PC=ON  → 只编 PC 端 (app_pc_360 / app_pc_720)
# TARGET_PC=OFF → 只编设备端 (app_360 / app_720)，默认
option(TARGET_PC "Build PC targets only (OFF = device targets only)" OFF)
if(TARGET_PC)
  message(STATUS "TARGET_PC=ON  → building PC targets only")
else()
  message(STATUS "TARGET_PC=OFF → building device targets only")
endif()

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
# libdrm 仅设备/Linux PC 需要（drm_warpper.h 的结构体类型）；Windows(mingw) 无此
# 包，PC 的 drm_warpper.h 走占位类型（见 #ifdef _WIN32）。
if(NOT WIN32)
  pkg_check_modules(LIBDRM REQUIRED libdrm)
endif()
pkg_check_modules(FREETYPE REQUIRED freetype2)
if(NOT TARGET_PC)
  pkg_check_modules(LIBEVDEV REQUIRED libevdev)
endif()

# 系统共享字体 (buildroot 的 epass-fonts 包)。找到就用它导出的目录, 编译期把
# FONT_REGISTRY_DIR 定成 /usr/share/fonts/epass, font_registry 直接读那里,
# app 不再自带字体。找不到 (纯本地 dev 构建, 无 staging) 则回退到随可执行文件
# 自带 res/fonts (下方 RES_FONT 那套)。
pkg_check_modules(EPASS_FONTS QUIET epass-fonts)
if(EPASS_FONTS_FOUND)
  pkg_get_variable(EPASS_FONTS_DIR epass-fonts fontsdir)
  message(STATUS "epass-fonts found, shared font dir: ${EPASS_FONTS_DIR}")
else()
  message(STATUS "epass-fonts not found, bundling fonts into res/fonts (dev fallback)")
endif()

# ============================================================================
# LVGL —— 优先用系统共享库, 否则下载 release 归档就地静态编 (无 submodule)
# ============================================================================
# 设备/buildroot 构建: staging 里有 pkg-config 的 lvgl (共享库 + 权威 lv_conf.h)。
# 找到就建一个同名 INTERFACE 目标 lvgl, 下游 target_link_libraries(... lvgl) 一行
# 不用改 —— 等价于把静态 .a 换成 -llvgl, 符号名不变。
# 找不到 (纯本地 PC 开发, 无 sysroot) 则退回 FetchContent 下载 v9.5.0 release 归档
# 就地静态编, 用本仓库自带 src/lv_conf.h 作配置源。
find_package(PkgConfig REQUIRED)
pkg_check_modules(LVGL IMPORTED_TARGET lvgl)
if(LVGL_FOUND)
  message(STATUS "LVGL: using system shared lib ${LVGL_VERSION} (${LVGL_LIBDIR})")
  # 同名 INTERFACE 目标 lvgl 转发系统库 (include/-L/-llvgl 都由 PkgConfig::LVGL 带);
  # 不能直接 target_link_libraries(lvgl INTERFACE ${LVGL_LIBRARIES}), 因为它值就是
  # 字符串 "lvgl" —— 会被当成对本目标自身的引用, -llvgl 反而丢掉。
  add_library(lvgl INTERFACE)
  target_link_libraries(lvgl INTERFACE PkgConfig::LVGL)
else()
  message(STATUS "LVGL: system lib not found, fetching v9.5.0 release archive (static)")
  include(FetchContent)
  set(LV_BUILD_CONF_PATH ${CMAKE_SOURCE_DIR}/src/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
  set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)
  set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build LVGL demos" FORCE)
  set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Use internal ThorVG (needs CXX)" FORCE)
  # 独立工具链可能没 C++ 前端: 把 CXX 指向 C 编译器并跳过探测 (反正没真 .cpp 要编)
  set(CMAKE_CXX_COMPILER ${CMAKE_C_COMPILER} CACHE FILEPATH "" FORCE)
  set(CMAKE_CXX_COMPILER_FORCED TRUE)
  # 解到名为 lvgl 的目录 (而非默认 _deps/lvgl-src), 好让下面把父目录加进 include 后
  # 工程里的 #include <lvgl/lvgl.h> 照旧能解析。
  FetchContent_Declare(lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_lvgl/lvgl)
  FetchContent_MakeAvailable(lvgl)
  # os_desktop.cmake 无条件把 src 下 .cpp (glTF/TFT_eSPI/LovyanGFX/vg_lite_tvg 桥接)
  # 塞进 lvgl 库, 摘掉以免那个假 CXX 去真编 C++。
  get_target_property(LVGL_SRCS lvgl SOURCES)
  list(FILTER LVGL_SRCS EXCLUDE REGEX "\\.cpp$")
  set_target_properties(lvgl PROPERTIES SOURCES "${LVGL_SRCS}")
  target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS})
  # LVGL 自身把源码根导出为 include (-> <lvgl.h>); 再把源码根的父目录加进来,
  # 让 <lvgl/lvgl.h> 这种带前缀的写法也能解析 (设备侧靠 buildroot 全局 -I/usr/include)。
  target_include_directories(lvgl INTERFACE ${CMAKE_BINARY_DIR}/_lvgl)
endif()

# ============================================================================
# 源文件
# ============================================================================
# 平台无关的公共源；显示/视频/按键三个后端缝按 target 追加：
#   设备 = drm_warpper.c + mediaplayer.c(+vdec) + key_enc_evdev.c
#   PC   = drm_warpper_sdl.c + mediaplayer_ffmpeg.c + key_sdl.c
set(EPASS_SOURCES
  src/main.c

  src/prts/prts.c
  src/prts/operators.c

  src/apps/apps.c
  src/apps/extmap.c
  src/apps/apps_cfg_parse.c
  src/apps/ipc_common.c

  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c
  src/render/fbdraw.c
  src/render/c8pal.c

  src/ui/font_registry.c
  src/ui/ui_theme.c
  src/ui/filemanager.c
  src/ui/battery.c
  src/ui/ipc_helper.c
  src/ui/uix_session.c

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
  src/ui_screens/screens/screen_usbselect.c
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

# 纯 POSIX 子系统：IPC(unix socket + epoll) 与 posix 定时器后端。设备端与 Linux
# PC 都用；Windows PC 交叉编译时剔除（apps.c/main.c 已 #ifdef _WIN32 stub 掉调用
# 侧，timer 换 win32 后端）。见下方 add_epass_pc_target 的 if(WIN32) 分流。
set(POSIX_ONLY_SOURCES
  src/apps/ipc_server.c
  src/apps/ipc_handler.c
  src/utils/timer_backend_posix.c
)

string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")

set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(FONT_DIR   ${CMAKE_SOURCE_DIR}/font)
file(GLOB RES_PNG  CONFIGURE_DEPENDS ${ASSETS_DIR}/*.png ${ASSETS_DIR}/*.c8)
file(GLOB RES_FONT CONFIGURE_DEPENDS ${FONT_DIR}/*.otf)

# ============================================================================
# 可执行 —— 一次产出两档分辨率 (360x640 / 720x1280)，源码共用，仅屏宏不同。
# name: 可执行名; screen_define: 屏宏 (覆盖 config.h 的兜底默认)
# ============================================================================
function(add_epass_target name screen_define)
  add_executable(${name} ${EPASS_SOURCES} ${POSIX_ONLY_SOURCES}
    # 设备后端：libdrm atomic + cedrus V4L2 + evdev
    src/driver/drm_warpper.c
    src/driver/key_enc_evdev.c
    src/render/mediaplayer.c
    src/vdec/mp4_demux.c
    src/vdec/bitreader.c
    src/vdec/nalu.c
    src/vdec/h264_parser.c
    src/vdec/h264_dpb.c
    src/vdec/vdec_v4l2.c
  )
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
  # 用共享字体时把字体目录烘进二进制 (font_registry 走 FONT_REGISTRY_DIR 分支)
  if(EPASS_FONTS_DIR)
    target_compile_definitions(${name} PRIVATE FONT_REGISTRY_DIR="${EPASS_FONTS_DIR}")
  endif()
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
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_PNG}  ${RES_OUT_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/fallback ${RES_OUT_DIR}/fallback
    COMMENT "Copying assets -> res/ next to ${name}"
    VERBATIM
  )
  # 仅无共享字体时才把字体自带进 res/fonts
  if(NOT EPASS_FONTS_FOUND)
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}/fonts
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_FONT} ${RES_OUT_DIR}/fonts/
      COMMENT "Bundling fonts -> res/fonts (dev fallback)"
      VERBATIM
    )
  endif()
endfunction()

# 仅 FetchContent 静态编时 lvgl 是可编译目标, 才能给它加编译选项;
# 系统共享库那条路 lvgl 是 INTERFACE 目标 (库已编好), 加 PRIVATE 属性会报错。
if(NOT LVGL_FOUND)
  target_compile_options(lvgl PRIVATE
    $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
  )
endif()

if(NOT TARGET_PC)
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
endif()

# ============================================================================
# PC Target —— 完整应用跑在 PC：drm_warpper 换 SDL 合成后端、视频换 ffmpeg
# 软解、按键换 SDL 键盘、字体走 fontconfig。数据目录见 config.h 的
# EPASS_PC_TARGET 块（缺省 ./pcdata）。仅 TARGET_PC=ON 时启用。
# ============================================================================
if(TARGET_PC)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale)

  function(add_epass_pc_target name screen_define)
    add_executable(${name} ${EPASS_SOURCES}
      # PC 后端：SDL 合成 + ffmpeg 解码 + SDL 键盘
      src/driver/drm_warpper_sdl.c
      src/driver/key_sdl.c
      src/render/mediaplayer_ffmpeg.c
    )
    # 平台相关：Windows 换 win32 定时器后端并剔除 IPC(unix socket)；Linux PC 同设备
    if(WIN32)
      target_sources(${name} PRIVATE src/utils/timer_backend_win32.c)
    else()
      target_sources(${name} PRIVATE ${POSIX_ONLY_SOURCES})
    endif()
    target_include_directories(${name} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens
      ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_screens/screens
      ${LIBDRM_INCLUDE_DIRS}   # drm_warpper.h 的结构体仍引 xf86drm 类型（仅头文件）
      ${PNG_INCLUDE_DIR}
      ${FREETYPE_INCLUDE_DIRS}
      ${SDL2_INCLUDE_DIRS}
      ${FFMPEG_INCLUDE_DIRS}
    )
    target_compile_definitions(${name} PRIVATE
      _DEFAULT_SOURCE
      ${screen_define}
      EPASS_PC_TARGET
      EPASS_GIT_VERSION="${GIT_COMMIT}"
      EPASS_GIT_TAG="${GIT_TAG}"
      COMPILE_TIME="${COMPILE_TIME}"
    )
    target_compile_options(${name} PRIVATE
      -Wall -Wextra -Wno-unused-parameter
      -Wno-format-truncation
      -ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/=/
      $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
    )
    # pkg-config 的库目录（交叉环境是 /mingw64/lib，不在 ld 默认搜索路径）
    target_link_directories(${name} PRIVATE
      ${SDL2_LIBRARY_DIRS}
      ${FFMPEG_LIBRARY_DIRS}
      ${FREETYPE_LIBRARY_DIRS}
    )
    # lvgl(静态库) 在前，其依赖的 freetype 等在后（宿主 ld 不做隐式二级依赖解析）
    # Windows(mingw) 无 dl/rt：dlopen 不用，clock_gettime 在 libc、定时器走 win32。
    set(_pc_extra_libs m pthread)
    if(NOT WIN32)
      list(APPEND _pc_extra_libs dl rt)
    endif()
    target_link_libraries(${name}
      lvgl
      ${PNG_LIBRARIES}
      ${FREETYPE_LIBRARIES}
      ${SDL2_LIBRARIES}
      ${FFMPEG_LIBRARIES}
      ${_pc_extra_libs}
    )
    # 资源拷贝同设备；fonts 恒自带（PC 字体解析直接走 res/fonts）
    # pcdata 骨架与可执行同级（运行时 ./pcdata，见 config.h EPASS_PC_TARGET）
    set(RES_OUT_DIR $<TARGET_FILE_DIR:${name}>/res)
    set(PCDATA_OUT_DIR $<TARGET_FILE_DIR:${name}>/pcdata)
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}/fonts
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_PNG}  ${RES_OUT_DIR}/
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_FONT} ${RES_OUT_DIR}/fonts/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/fallback ${RES_OUT_DIR}/fallback
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PCDATA_OUT_DIR}/assets
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PCDATA_OUT_DIR}/app
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PCDATA_OUT_DIR}/dispimg
      COMMENT "Copying assets -> res/ and ensuring pcdata/{assets,app,dispimg} next to ${name}"
      VERBATIM
    )
  endfunction()

  add_epass_pc_target(app_pc_360 USE_360_640_SCREEN)
  add_epass_pc_target(app_pc_720 USE_720_1280_SCREEN)
  message(STATUS "PC targets enabled: app_pc_360 / app_pc_720")
endif()

# ============================================================================
# 安装规则 (可选)：bin/app_* 与 bin/res/ 同级，保持运行时相对结构
# ============================================================================
if(NOT TARGET_PC)
  install(TARGETS app_360 app_720 DESTINATION bin)
  install(DIRECTORY ${ASSETS_DIR}/ DESTINATION bin/res
          FILES_MATCHING PATTERN "*.png" PATTERN "*.c8")
  if(NOT EPASS_FONTS_FOUND)
    install(FILES ${RES_FONT} DESTINATION bin/res/fonts)
  endif()
  install(DIRECTORY ${ASSETS_DIR}/fallback DESTINATION bin/res)
endif()
