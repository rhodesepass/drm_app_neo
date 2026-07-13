#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif
#include "utils/cJSON.h"
#include "utils/misc.h"
#include "utils/compat.h"
#include "config.h"

uint64_t get_now_us(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

uint64_t get_mono_us(void){
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (uint64_t)cnt.QuadPart * 1000000ULL / (uint64_t)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

void fill_nv12_buffer_with_color(uint8_t* buf, int width, int height, uint32_t rgb){
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    uint8_t y = (uint8_t)(( 77 * r + 150 * g +  29 * b ) >> 8);
    uint8_t u = (uint8_t)(((-43 * r - 85 * g + 128 * b) >> 8) + 128);
    uint8_t v = (uint8_t)(((128 * r - 107 * g - 21 * b) >> 8) + 128);

    int y_size = width * height;
    int uv_size = width * height / 2;

    for(int i = 0; i < y_size; i++){
        buf[i] = y;
    }

    for(int i = 0; i < uv_size; i += 2){
        buf[y_size + i] = u;
        buf[y_size + i + 1] = v;
    }
}


void safe_strcpy(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

int join_path(char *dst, size_t dst_sz, const char *base, const char *rel) {
    if (!dst || dst_sz == 0) return -1;
    dst[0] = '\0';

    if (!rel || rel[0] == '\0') return -1;

    // 绝对路径
    if (rel[0] == '/') {
        safe_strcpy(dst, dst_sz, rel);
        return 0;
    }

    if (!base || base[0] == '\0') {
        safe_strcpy(dst, dst_sz, rel);
        return 0;
    }

    // 避免重复的 '/'
    size_t base_len = strlen(base);
    if (base[base_len - 1] == '/') {
        snprintf(dst, dst_sz, "%s%s", base, rel);
    } else {
        snprintf(dst, dst_sz, "%s/%s", base, rel);
    }
    return 0;
}

const char* path_basename(const char *path) {
    if (!path) return "";
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') len--;
    if (len == 0) return "";

    const char *end = path + len;
    const char *p = end;
    while (p > path && *(p - 1) != '/') p--;
    return p;
}

int file_exists_readable(const char *filepath) {
    if (!filepath || filepath[0] == '\0') return 0;
    return access(filepath, R_OK) == 0;
}

int file_exists_executable(const char *filepath) {
    if (!filepath || filepath[0] == '\0') return 0;
    return access(filepath, X_OK) == 0;
}

bool path_is_dir(const char *path) {
    if (!path || path[0] == '\0') return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool path_is_file(const char *path) {
    if (!path || path[0] == '\0') return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

uint64_t fs_avail_bytes(const char *mp) {
#ifdef _WIN32
    ULARGE_INTEGER avail;
    if (!GetDiskFreeSpaceExA(mp, &avail, NULL, NULL)) return 0;
    return (uint64_t)avail.QuadPart;
#else
    struct statvfs s;
    if (statvfs(mp, &s) != 0) return 0;
    return (uint64_t)s.f_bavail * s.f_bsize;
#endif
}

uint64_t fs_total_bytes(const char *mp) {
#ifdef _WIN32
    ULARGE_INTEGER total;
    if (!GetDiskFreeSpaceExA(mp, NULL, &total, NULL)) return 0;
    return (uint64_t)total.QuadPart;
#else
    struct statvfs s;
    if (statvfs(mp, &s) != 0) return 0;
    return (uint64_t)s.f_blocks * s.f_bsize;
#endif
}

void set_lvgl_path(char *dst, size_t dst_sz, const char *abs_path) {
    if (!dst || dst_sz == 0) return;
    if (!abs_path || abs_path[0] == '\0') {
        dst[0] = '\0';
        return;
    }
    // 剥掉已有 LVGL 盘符(仅 'A'，见 lv_conf LV_FS_STDIO_LETTER)，再规范化。
    // 只认 'A' 而非任意字母：Windows 真实盘符 C:\ 不能被误当 LVGL 盘符剥掉。
    const char *raw = abs_path;
    if (abs_path[0] == 'A' && abs_path[1] == ':') {
        raw = abs_path + 2;
    }
    // LV_FS_STDIO_PATH 为 "/"，相对路径会被拼成 "/./pcdata/..." → 打不开。
    // realpath 成绝对路径后再加 A:（文件须已存在；失败则原样回退）。
    char resolved[PATH_MAX];
    const char *use = raw;
    if (realpath(raw, resolved))
        use = resolved;
    snprintf(dst, dst_sz, "A:%s", use);
}

char* read_file_all(const char *filepath, size_t *out_len) {
    if (out_len) *out_len = 0;

    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char *buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (n != (size_t)sz) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    if (out_len) *out_len = n;
    return buf;
}

const char* json_get_string(cJSON *obj, const char *key) {
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (!it || !cJSON_IsString(it) || !it->valuestring) return NULL;
    return it->valuestring;
}

int json_get_int(cJSON *obj, const char *key, int def) {
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (!it || !cJSON_IsNumber(it)) return def;
    return it->valueint;
}

bool json_get_bool(cJSON *obj, const char *key, bool def) {
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (!it || !cJSON_IsBool(it)) return def;
    return cJSON_IsTrue(it);
}

// "#RRGGBB" -> 0x00RRGGBB (opinfo color expects alpha in draw stage)
static uint32_t parse_rgb00(const char *hex) {
    if (!hex) return 0;
    if (hex[0] == '#') hex++;
    if (strlen(hex) != 6) return 0;
    unsigned int v = 0;
    if (sscanf(hex, "%06x", &v) != 1) return 0;
    return (uint32_t)(v & 0x00FFFFFFu);
}

// "#RRGGBB" -> 0xFFRRGGBB
uint32_t parse_rgbff(const char *hex) {
    return 0xFF000000u | parse_rgb00(hex);
}

int is_hex_color_6(const char *s) {
    if (!s) return 0;
    if (s[0] != '#') return 0;
    if (strlen(s) != 7) return 0;
    for (int i = 1; i < 7; i++) {
        char c = s[i];
        if (!isxdigit((unsigned char)c)) return 0;
    }
    return 1;
}


// 外置 SD 数据分区设备节点：按 /proc/cmdline 区分 NAND/SD 启动 (见 config.h 注释)
const char* sd_dev_path(void){
    static int cached = -1; // -1=未探测 0=NAND启动 1=SD启动
    if (cached < 0) {
        cached = 0;
        char cmdline[512] = {0};
        FILE *f = fopen("/proc/cmdline", "r");
        if (f) {
            size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
            cmdline[n] = '\0';
            fclose(f);
            if (strstr(cmdline, SD_BOOT_CMDLINE_SIGN)) cached = 1;
        }
    }
    return cached ? SD_DEV_PATH_SD_BOOT : SD_DEV_PATH_NAND_BOOT;
}

bool is_sdcard_inserted(){
    return access(sd_dev_path(), F_OK) == 0;
}

// /sd 是否已挂载 (由 init 脚本负责挂载，这里只探测系统状态)
bool is_sd_mounted(void){
    FILE *f = fopen("/proc/mounts", "r");
    if (f == NULL) return false;
    bool found = false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char dev[128], mp[128];
        if (sscanf(line, "%127s %127s", dev, mp) == 2 &&
            strcmp(mp, SD_MOUNT_POINT) == 0) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

void parse_log_file(FILE* parse_log_f,const char *path, const char *message, parse_log_type_t type){
    if(parse_log_f == NULL){
        return;
    }
    switch(type){
        case PARSE_LOG_ERROR:
            fprintf(parse_log_f, "在处理%s时发生错误: %s\n", path, message);
            break;
        case PARSE_LOG_WARN:
            fprintf(parse_log_f, "在处理%s时发生警告: %s\n", path, message);
            break;
    }
    fflush(parse_log_f);
}