cmake_minimum_required(VERSION 3.10)
project(epass_ui_sim C)

# 仓库根目录 (sim 的上一级)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 依赖
# ============================================================================
find_package(PkgConfig REQUIRED)
find_package(PNG REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# ============================================================================
# LVGL (用 sim 专用的 lv_conf.h: 开 SDL + FreeType)
# ============================================================================
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE PATH "lv_conf.h path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
add_subdirectory(${REPO}/lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

# ============================================================================
# 模拟器可执行
# ============================================================================
add_executable(epass_ui_sim
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_backend.c
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_plane.c

  ${REPO}/src/ui_screens/screen_manager.c
  ${REPO}/src/ui_screens/screens/screen_mainmenu.c
  ${REPO}/src/ui_screens/styles.c
  ${REPO}/src/ui/font_registry.c
  ${REPO}/src/utils/log.c
)

target_include_directories(epass_ui_sim PRIVATE
  ${REPO}
  ${REPO}/lvgl
  ${REPO}/src
  ${REPO}/src/ui_screens
  ${REPO}/src/ui_screens/screens
  ${REPO}/eez_design/icon_header_gen
  ${SDL2_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
)

# 屏目标由构建注入 (覆盖 config.h 的兜底默认)：cmake -DEPASS_SCREEN=720x1280
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

# 字体目录: FreeType 直接读, 不走 lv_fs, 无盘符前缀
# 图片目录: 走 lv_fs (libpng 解码), 需带盘符 'A:' 前缀
target_compile_definitions(epass_ui_sim PRIVATE
  ${EPASS_SCREEN_DEFINE}
  FONT_REGISTRY_DIR="${REPO}/font"
  UI_IMG_DIR="A:${REPO}/font"
)

target_compile_options(epass_ui_sim PRIVATE -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(epass_ui_sim
  lvgl
  ${SDL2_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${PNG_LIBRARIES}
  m
  pthread
)
