/**
 * @file apps.c
 * @brief 用户应用管理模块 - 实现
 *
 * 扫描 /apps/ 和 /sd/apps/ 目录，解析 appconfig.json 配置文件。
 * 参考 prts/operators.c 的实现风格。
 */

#include "apps/apps.h"
#include "utils/cJSON.h"
#include "utils/log.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "utils/misc.h"

// ========== 日志函数 ==========

void apps_log(apps_manager_t* mgr, const char* path,
              const char* message, app_log_type_t type) {
    if (!mgr || !mgr->parse_log) return;

    const char* type_str;
    switch (type) {
        case APP_LOG_ERROR: type_str = "ERROR"; break;
        case APP_LOG_WARN:  type_str = "WARN";  break;
        case APP_LOG_INFO:  type_str = "INFO";  break;
        default:            type_str = "UNKNOWN"; break;
    }

    fprintf(mgr->parse_log, "[%s] %s: %s\n", type_str, path ? path : "?", message);
    fflush(mgr->parse_log);

    // 同时输出到系统日志
    switch (type) {
        case APP_LOG_ERROR:
            log_error("[APPS] %s: %s", path ? path : "?", message);
            break;
        case APP_LOG_WARN:
            log_warn("[APPS] %s: %s", path ? path : "?", message);
            break;
        default:
            log_info("[APPS] %s: %s", path ? path : "?", message);
            break;
    }
}

// ========== 应用加载 ==========

/**
 * @brief 尝试从目录加载一个应用
 * @return 0成功，-1失败
 */
static int app_try_load(apps_manager_t* mgr, app_entry_t* app,
                        const char* app_dir, app_source_t source) {
    if (!app || !app_dir) return -1;

    // 初始化条目
    memset(app, 0, sizeof(*app));
    app->pid = -1;
    app->source = source;

    // 保存应用目录
    safe_strcpy(app->app_dir, sizeof(app->app_dir), app_dir);

    // 构建配置文件路径
    char config_path[APP_PATH_MAX_LEN];
    snprintf(config_path, sizeof(config_path), "%s/%s", app_dir, APPS_CONFIG_FILENAME);

    // 读取配置文件
    size_t json_len = 0;
    char* buf = read_file_all(config_path, &json_len);
    if (!buf) {
        apps_log(mgr, app_dir, "appconfig.json not found or unreadable", APP_LOG_ERROR);
        return -1;
    }

    // 解析 JSON
    cJSON* json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        apps_log(mgr, app_dir, "appconfig.json parse failed", APP_LOG_ERROR);
        return -1;
    }

    // 验证版本
    int version = json_get_int(json, "version", -1);
    if (version != APPS_CONFIG_VERSION) {
        apps_log(mgr, app_dir, "Version mismatch", APP_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 解析名称（必需）
    const char* name = json_get_string(json, "name");
    if (!name || name[0] == '\0') {
        apps_log(mgr, app_dir, "Missing app name", APP_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    safe_strcpy(app->name, sizeof(app->name), name);

    // 解析可选字段
    const char* desc = json_get_string(json, "description");
    safe_strcpy(app->description, sizeof(app->description),
                desc ? desc : "");

    const char* author = json_get_string(json, "author");
    safe_strcpy(app->author, sizeof(app->author),
                author ? author : "Unknown");

    const char* app_ver = json_get_string(json, "app_version");
    safe_strcpy(app->app_version, sizeof(app->app_version),
                app_ver ? app_ver : "1.0.0");

    // 验证屏幕分辨率
    const char* screen = json_get_string(json, "screen");
    if (!screen || screen[0] == '\0') {
        apps_log(mgr, app_dir, "Missing screen field", APP_LOG_WARN);
        // 不作为错误，继续加载
    }
#if defined(USE_360_640_SCREEN)
    else if (strcmp(screen, "360x640") != 0) {
        apps_log(mgr, app_dir, "Screen resolution mismatch (expected 360x640)", APP_LOG_WARN);
    }
#endif

    // 解析可执行文件（必需）
    cJSON* exec_obj = cJSON_GetObjectItem(json, "executable");
    const char* exec_file = NULL;

    if (exec_obj && cJSON_IsObject(exec_obj)) {
        exec_file = json_get_string(exec_obj, "file");
    } else {
        // 兼容简单格式
        exec_file = json_get_string(json, "executable");
    }

    if (!exec_file || exec_file[0] == '\0') {
        apps_log(mgr, app_dir, "Missing executable file", APP_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 构建可执行文件完整路径
    join_path(app->exec_path, sizeof(app->exec_path), app_dir, exec_file);

    // 验证可执行文件存在
    if (!file_exists_readable(app->exec_path)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Executable not found: %s", exec_file);
        apps_log(mgr, app_dir, msg, APP_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 解析图标（可选）
    const char* icon = json_get_string(json, "icon");
    if (icon && icon[0] != '\0') {
        char abs_icon[APP_PATH_MAX_LEN];
        join_path(abs_icon, sizeof(abs_icon), app_dir, icon);
        if (file_exists_readable(abs_icon)) {
            set_lvgl_path(app->icon_path, sizeof(app->icon_path), abs_icon);
        } else {
            apps_log(mgr, app_dir, "Icon file not found, using default", APP_LOG_WARN);
            safe_strcpy(app->icon_path, sizeof(app->icon_path), APPS_DEFAULT_ICON_PATH);
        }
    } else {
        safe_strcpy(app->icon_path, sizeof(app->icon_path), APPS_DEFAULT_ICON_PATH);
    }

    cJSON_Delete(json);

    apps_log(mgr, app_dir, "Application loaded successfully", APP_LOG_INFO);
    return 0;
}

/**
 * @brief 扫描单个目录
 * @return 错误计数
 */
static int apps_scan_directory(apps_manager_t* mgr, const char* dirpath, app_source_t source) {
    int error_cnt = 0;

    DIR* dir = opendir(dirpath);
    if (!dir) {
        // 目录不存在不是错误
        log_info("[APPS] Directory not accessible: %s", dirpath);
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过非目录
        if (entry->d_type != DT_DIR) continue;

        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;

        // 检查容量
        if (mgr->count >= APPS_MAX_COUNT) {
            apps_log(mgr, dirpath, "Maximum app count reached", APP_LOG_WARN);
            break;
        }

        // 构建完整路径
        char app_path[APP_PATH_MAX_LEN];
        snprintf(app_path, sizeof(app_path), "%s%s", dirpath, entry->d_name);

        // 尝试加载
        if (app_try_load(mgr, &mgr->apps[mgr->count], app_path, source) == 0) {
            mgr->apps[mgr->count].index = mgr->count;
            mgr->count++;
        } else {
            error_cnt++;
        }
    }

    closedir(dir);
    return error_cnt;
}

// ========== 公共接口 ==========

int apps_manager_init(apps_manager_t* mgr) {
    if (!mgr) return -1;

    log_info("==> Apps Manager Initializing...");

    memset(mgr, 0, sizeof(*mgr));
    mgr->running_index = -1;
    mgr->state = APP_STATE_IDLE;

    // 打开解析日志
    mgr->parse_log = fopen(APPS_PARSE_LOG_PATH, "w");
    if (!mgr->parse_log) {
        log_warn("Failed to open apps parse log: %s", APPS_PARSE_LOG_PATH);
    }

    log_info("==> Apps Manager Initialized!");
    return 0;
}

void apps_manager_destroy(apps_manager_t* mgr) {
    if (!mgr) return;

    if (mgr->parse_log) {
        fclose(mgr->parse_log);
        mgr->parse_log = NULL;
    }

    memset(mgr, 0, sizeof(*mgr));
}

int apps_scan(apps_manager_t* mgr, bool use_sd) {
    if (!mgr) return 0;

    // 清空现有列表
    mgr->count = 0;

    int err_cnt = 0;

    // 扫描 NAND 应用
    log_info("==> Scanning NAND apps: %s", APPS_DIR_NAND);
    err_cnt += apps_scan_directory(mgr, APPS_DIR_NAND, APP_SOURCE_NAND);

    // 扫描 SD 卡应用
    if (use_sd) {
        log_info("==> Scanning SD apps: %s", APPS_DIR_SD);
        err_cnt += apps_scan_directory(mgr, APPS_DIR_SD, APP_SOURCE_SD);
    }

    if (err_cnt > 0) {
        log_warn("Apps scan completed with %d errors", err_cnt);
    }

    log_info("==> Apps scan complete! Found %d apps", mgr->count);
    return mgr->count;
}

app_entry_t* apps_get(apps_manager_t* mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->apps[index];
}

int apps_get_count(apps_manager_t* mgr) {
    return mgr ? mgr->count : 0;
}

bool apps_is_running(apps_manager_t* mgr) {
    return mgr && mgr->running_index >= 0;
}
