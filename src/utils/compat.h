#pragma once

// Windows(mingw-w64) 与 POSIX 的零碎差异垫片。非 Windows 下整个文件为空，
// 对设备/Linux PC 构建无任何影响。需要 usleep/mkdir/realpath/access 的 .c
// 文件顶部 include 本头即可。
#ifdef _WIN32

#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>

// mingw 自带 usleep（unistd.h），无需垫。

// mkdir(path,mode) 两参 → _mkdir(path) 单参（Windows 无 POSIX 权限位）。
#define mkdir(path, mode) _mkdir(path)

// realpath(path,resolved) → _fullpath(resolved,path,...)，参数顺序相反。
// resolved 缓冲需 >= MAX_PATH。
#define realpath(path, resolved) _fullpath((resolved), (path), MAX_PATH)

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

// access() 权限位：Windows 的 _access 无执行位，X_OK 退化为存在性检查。
#ifndef F_OK
#define F_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef X_OK
#define X_OK 0
#endif

// Windows 无 O_SYNC（同步写盘语义）。设备侧 sysfs 读用它，Windows 上退化为 0。
#ifndef O_SYNC
#define O_SYNC 0
#endif

#endif // _WIN32
