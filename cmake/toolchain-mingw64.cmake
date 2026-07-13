# mingw-w64 交叉编译 toolchain（x86_64 Windows）。
# 工具链用 Linux 原生的 x86_64-w64-mingw32-gcc；依赖库(SDL2/ffmpeg/freetype/
# libpng…)来自 MSYS2 预编译包，装在 MINGW_SYSROOT（默认 /mingw64）。
# 用法：cmake -DTARGET_PC=ON --toolchain cmake/toolchain-mingw64.cmake ...

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
# 工程无真正的 .cpp（ThorVG 已关，LVGL 的 .cpp 在主 CMakeLists 里被摘掉），
# 沿用假 CXX 技巧：把 CXX 指向 C 编译器，省得工具链非要带 C++ 前端。
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

# MSYS2 依赖 sysroot（Dockerfile 里把 mingw64 包装到这里）
if(NOT DEFINED MINGW_SYSROOT)
  set(MINGW_SYSROOT "/mingw64")
endif()

set(CMAKE_FIND_ROOT_PATH ${MINGW_SYSROOT} /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config 只在 sysroot 里找 .pc，且不加 sysroot 前缀（MSYS2 包内路径已是 /mingw64/…）
set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_SYSROOT}/lib/pkgconfig:${MINGW_SYSROOT}/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
