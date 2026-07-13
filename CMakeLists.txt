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
pkg_check_modules(LIBDRM REQUIRED libdrm)
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
# LVGL 配置
# ============================================================================
# 设置为 CACHE 变量以覆盖 LVGL 的默认值
set(LV_BUILD_CONF_PATH ${CMAKE_SOURCE_DIR}/src/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build LVGL demos" FORCE)
# lv_conf.h 里 LV_USE_THORVG_INTERNAL 已是 0；这里同步关掉 CMake 侧同名开关，
# 否则 ThorVG 的 67 个 .cpp 会被无条件编译，逼着交叉工具链必须带 C++ 前端。
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Use internal ThorVG (needs CXX)" FORCE)
# lvgl/CMakeLists.txt 顶层无条件 enable_language(CXX ASM)，但工程里唯一用到
# C++ 的地方就是上面关掉的 ThorVG。为了不强依赖交叉工具链自带 C++ 前端
# (cc1plus)，把 CXX“编译器”指向 C 编译器本身，并跳过 CMake 的 try_compile
# 探测——反正不会有真的 .cpp 落到它头上.....大概吧？！？！
# 我是说，不会吧不会吧，不会真的有人用CPP吧？！
set(CMAKE_CXX_COMPILER ${CMAKE_C_COMPILER} CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
add_subdirectory(lvgl)

# 不是哥们，你TM......
# LVGL 自身的 os_desktop.cmake 把 src/ 下所有 .cpp（glTF viewer、TFT_eSPI、
# LovyanGFX、vg_lite_tvg 等可选桥接，共二十来个，跟 lv_conf.h 的开关无关）
# 无条件塞进主 lvgl 库，逼着上面那个假 CXX 编译器去真编译，必炸。
# add_subdirectory 之后从 target 的 SOURCES 属性里把 .cpp 全部摘掉。
# （不能用 git apply 打 patch：buildroot 打包源码会剥掉 .git，git 会向上
# 认领到外层仓库，patch 静默失败。）
get_target_property(LVGL_SRCS lvgl SOURCES)
list(FILTER LVGL_SRCS EXCLUDE REGEX "\\.cpp$")
set_target_properties(lvgl PROPERTIES SOURCES "${LVGL_SRCS}")
# LVGL 开启 LV_USE_FREETYPE 后，其适配源需要 freetype 头
target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS})

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
  src/apps/ipc_server.c
  src/apps/ipc_handler.c

  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c
  src/render/fbdraw.c

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
  add_executable(${name} ${EPASS_SOURCES}
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

target_compile_options(lvgl PRIVATE
  $<$<BOOL:${APP_RELEASE}>:-DAPP_RELEASE>
)

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
  pkg_check_modules(FONTCONFIG REQUIRED fontconfig)

  function(add_epass_pc_target name screen_define)
    add_executable(${name} ${EPASS_SOURCES}
      # PC 后端：SDL 合成 + ffmpeg 解码 + SDL 键盘
      src/driver/drm_warpper_sdl.c
      src/driver/key_sdl.c
      src/render/mediaplayer_ffmpeg.c
    )
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
      ${FONTCONFIG_INCLUDE_DIRS}
    )
    target_compile_definitions(${name} PRIVATE
      _DEFAULT_SOURCE
      ${screen_define}
      EPASS_PC_TARGET
      FONT_REGISTRY_USE_FONTCONFIG
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
    # lvgl(静态库) 在前，其依赖的 freetype 等在后（宿主 ld 不做隐式二级依赖解析）
    target_link_libraries(${name}
      lvgl
      ${PNG_LIBRARIES}
      ${FREETYPE_LIBRARIES}
      ${SDL2_LIBRARIES}
      ${FFMPEG_LIBRARIES}
      ${FONTCONFIG_LIBRARIES}
      m
      dl
      pthread
      rt
    )
    # 资源拷贝同设备；fonts 恒自带（fontconfig 未命中时的精确回退源）
    set(RES_OUT_DIR $<TARGET_FILE_DIR:${name}>/res)
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OUT_DIR}/fonts
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_PNG}  ${RES_OUT_DIR}/
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RES_FONT} ${RES_OUT_DIR}/fonts/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/fallback ${RES_OUT_DIR}/fallback
      COMMENT "Copying assets -> res/ next to ${name}"
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
          FILES_MATCHING PATTERN "*.png")
  if(NOT EPASS_FONTS_FOUND)
    install(FILES ${RES_FONT} DESTINATION bin/res/fonts)
  endif()
  install(DIRECTORY ${ASSETS_DIR}/fallback DESTINATION bin/res)
endif()
