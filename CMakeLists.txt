cmake_minimum_required(VERSION 3.10)
project(epass_drm_app)
include(FetchContent)

find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Git version: ${GIT_VERSION}")

# set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(LIBDRM REQUIRED libdrm)

set(LV_BUILD_CONF_PATH ${CMAKE_SOURCE_DIR}/src/lv_conf.h)
add_subdirectory(lvgl)


FILE(GLOB UI_SOURCES "src/ui/*.c")

add_executable(epass_drm_app
  src/main.c

  src/prts/prts.c

  src/driver/drm_warpper.c
  src/driver/key_enc_evdev.c

  src/render/mediaplayer.c
  src/render/layer_animation.c
  src/render/lvgl_drm_warp.c

  ${UI_SOURCES}

  src/overlay/overlay.c


  src/utils/spsc_queue.c
  src/utils/log.c
  src/utils/timer.c


)

target_include_directories(epass_drm_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/prts
  ${CMAKE_CURRENT_SOURCE_DIR}/src/driver
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
  ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay
  ${LIBEVDEV_INCLUDE_DIRS}
  ${LIBDRM_INCLUDE_DIRS}
)

target_compile_definitions(epass_drm_app PRIVATE
  _DEFAULT_SOURCE
)

target_compile_definitions(epass_drm_app PRIVATE 
 EPASS_GIT_VERSION="${GIT_VERSION}"
)

target_compile_options(epass_drm_app PRIVATE
  -Wall -Wextra -Wno-unused-parameter
  -ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/=/
   ${LIBEVDEV_CFLAGS_OTHER}
)

include_directories(${PNG_INCLUDE_DIR})
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/base/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/parser/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/stream/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/external/include/adecoder)


set(CMAKE_LIBRARY_PATH "/home/shirogane/Desktop/liulianpai/cedarx_build/lib")


target_link_libraries(epass_drm_app
  m
  MemAdapter
  vdecoder
  VE
  videoengine
  cdc_base 
  dl
  pthread
  cdx_parser
  cdx_stream
  cdx_base
  cdx_common
  rt

  ${LIBEVDEV_LIBRARIES}
  ${LIBDRM_LIBRARIES}
  ${PNG_LIBRARIES}

  lvgl
)


