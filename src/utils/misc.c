#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include "utils/cJSON.h"
#include "utils/misc.h"

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

void set_lvgl_path(char *dst, size_t dst_sz, const char *abs_path) {
    if (!dst || dst_sz == 0) return;
    if (!abs_path || abs_path[0] == '\0') {
        dst[0] = '\0';
        return;
    }
    // 已经是 LVGL 格式
    if (isalpha((unsigned char)abs_path[0]) && abs_path[1] == ':') {
        safe_strcpy(dst, dst_sz, abs_path);
        return;
    }
    snprintf(dst, dst_sz, "A:%s", abs_path);
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
