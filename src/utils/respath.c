#include "utils/respath.h"
#include "utils/log.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define RESPATH_MAX 512
#define RESPATH_RING 4

// 定位失败时的兜底, 维持旧设备布局, 避免直接炸。
static char s_res_dir[RESPATH_MAX] = "/root/" RES_SUBDIR;

void respath_init(void)
{
    char exe[RESPATH_MAX];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
    if (n == 0 || n >= sizeof(exe) - 1) {
        log_warn("respath: GetModuleFileNameA failed, fallback to %s", s_res_dir);
        return;
    }
    exe[n] = '\0';
    // 截目录：Windows 路径分隔符可能是 \ 或 /，取最后一个。
    char *slash = strrchr(exe, '\\');
    char *fwd = strrchr(exe, '/');
    if (fwd > slash) slash = fwd;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) {
        log_warn("respath: readlink /proc/self/exe failed, fallback to %s", s_res_dir);
        return;
    }
    exe[n] = '\0';
    char *slash = strrchr(exe, '/');
#endif
    if (slash) {
        *slash = '\0';
    }
    snprintf(s_res_dir, sizeof(s_res_dir), "%s/%s", exe, RES_SUBDIR);
    log_info("respath: resource dir = %s", s_res_dir);
}

const char *respath_dir(void)
{
    return s_res_dir;
}

static char *ring_next(void)
{
    static char buf[RESPATH_RING][RESPATH_MAX];
    static int idx = 0;
    idx = (idx + 1) % RESPATH_RING;
    return buf[idx];
}

const char *respath(const char *rel)
{
    char *b = ring_next();
    snprintf(b, RESPATH_MAX, "%s/%s", s_res_dir, rel);
    return b;
}

const char *respath_lvfs(const char *rel)
{
    char *b = ring_next();
    snprintf(b, RESPATH_MAX, "A:%s/%s", s_res_dir, rel);
    return b;
}
