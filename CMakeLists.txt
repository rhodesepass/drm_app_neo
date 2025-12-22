cmake_minimum_required(VERSION 3.10)
project(epass_drm_app)

find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Git version: ${GIT_VERSION}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)


add_executable(epass_drm_app
  src/main.c
  src/drm_warpper.c
  src/log.c
  src/mediaplayer.c
  src/virt_to_phys.c
)

target_include_directories(epass_drm_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/RREFont
  ${LIBEVDEV_INCLUDE_DIRS}
)

target_compile_definitions(epass_drm_app PRIVATE
  _DEFAULT_SOURCE
)

target_compile_definitions(epass_drm_app PRIVATE 
 EPASS_GIT_VERSION="${GIT_VERSION}"
)

target_compile_options(epass_drm_app PRIVATE
  -Wall -Wextra -Wpedantic -Wno-unused-parameter
   ${LIBEVDEV_CFLAGS_OTHER}
)

include_directories(/home/shirogane/Desktop/liulianpai/buildroot-tiny200/output/host/arm-buildroot-linux-gnueabi/sysroot/usr/include/drm/)

include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/base/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/parser/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/libcore/stream/include)
include_directories(/home/shirogane/Desktop/liulianpai/drm_app/backup/tina_multimedia/libcedarx/external/include/adecoder)


set(CMAKE_LIBRARY_PATH "/home/shirogane/Desktop/liulianpai/cedarx_build/lib")


target_link_libraries(epass_drm_app
  m
  drm
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

  ${LIBEVDEV_LIBRARIES}
)


