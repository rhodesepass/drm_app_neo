#include "apps/apps.h"
#include "apps/apps_cfg_parse.h"
#include "utils/log.h"
#include "utils/misc.h"
#include "utils/cJSON.h"
#include "config.h"
#include "utils/timer.h"
#include "utils/uuid.h"
#include <apps/extmap.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>


int apps_cfg_try_load(apps_t *apps,app_entry_t* app,char * app_dir,app_source_t source,int index){
    if (!app || !app_dir) return -1;

    // 初始化条目
    memset(app, 0, sizeof(*app));
    app->pid = -1;
    app->source = source;
    app->index = index;

    // 保存应用目录
    safe_strcpy(app->app_dir, sizeof(app->app_dir), app_dir);

    // 构建配置文件路径
    char config_path[256];
    join_path(config_path, sizeof(config_path), app_dir, APPS_CONFIG_FILENAME);

    // 读取配置文件
    size_t json_len = 0;
    char* buf = read_file_all(config_path, &json_len);
    if (!buf) {
        parse_log_file(apps->parse_log_f, app_dir, APPS_CONFIG_FILENAME "不存在或读取失败", PARSE_LOG_ERROR);
        return -1;
    }

    // 解析 JSON
    cJSON* json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        parse_log_file(apps->parse_log_f, app_dir, APPS_CONFIG_FILENAME "解析失败", PARSE_LOG_ERROR);
        return -1;
    }

    // 验证版本
    int version = json_get_int(json, "version", -1);
    if (version != APPS_CONFIG_VERSION) {
        parse_log_file(apps->parse_log_f, app_dir,  "版本不匹配", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 解析名称（没有则用文件夹名称）
    const char *name = json_get_string(json, "name");
    if (!name || name[0] == '\0') {
        // name 可选：没有就用文件夹名称
        name = path_basename(app_dir);
    }
    safe_strcpy(app->app_name, sizeof(app->app_name), name);

    // 解析 uuid

    const char *uuid_str = json_get_string(json, "uuid");
    if (!uuid_str || uuid_str[0] == '\0') {
        parse_log_file(apps->parse_log_f, app_dir, "缺少字段 uuid", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    if (uuid_parse(uuid_str, &app->uuid) != 0) {
        parse_log_file(apps->parse_log_f, app_dir, "uuid 解析失败", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

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
        parse_log_file(apps->parse_log_f, app_dir, "缺少可执行文件", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 构建可执行文件完整路径
    join_path(app->executable_path, sizeof(app->executable_path), app_dir, exec_file);

    // 验证可执行文件存在
    if (!file_exists_readable(app->executable_path)) {
        parse_log_file(apps->parse_log_f, app_dir, "可执行文件不存在", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 添加执行权限
    if (chmod(app->executable_path, 0755) != 0) {
        parse_log_file(apps->parse_log_f, app_dir, "添加执行权限失败", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 解析type
    const char* type = json_get_string(json, "type");
    if (type && type[0] != '\0') {
        if (strcmp(type, "bg") == 0) {
            app->type = APP_TYPE_BACKGROUND;
        }
        else if (strcmp(type, "fg") == 0) {
            app->type = APP_TYPE_FOREGROUND;
        }
        else if (strcmp(type, "fg_ext") == 0) {
            app->type = APP_TYPE_FOREGROUND_EXTENSION_ONLY;
        }
        else{
            parse_log_file(apps->parse_log_f, app_dir, "type 不合法", PARSE_LOG_ERROR);
            cJSON_Delete(json);
            return -1;
        }
    }
    else{
        parse_log_file(apps->parse_log_f, app_dir, "缺少字段 type", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // 解析screen
    bool screen_match_this_build = false;

    cJSON* screens_obj = cJSON_GetObjectItem(json, "screens");
    if (screens_obj && cJSON_IsArray(screens_obj)) {
        for(int i = 0; i < cJSON_GetArraySize(screens_obj); i++){
            cJSON* screen_obj = cJSON_GetArrayItem(screens_obj, i);
            if (screen_obj && cJSON_IsString(screen_obj)) {
#ifdef USE_360_640_SCREEN
                if (strcmp(screen_obj->valuestring, "360x640") == 0) {
                    screen_match_this_build = true;
                }
#endif
#ifdef USE_480_854_SCREEN
                if (strcmp(screen_obj->valuestring, "480x854") == 0) {
                    screen_match_this_build = true;
                }
#endif
#ifdef USE_720_1280_SCREEN
                if (strcmp(screen_obj->valuestring, "720x1280") == 0) {
                    screen_match_this_build = true;
                }
#endif
            }
            else{
                parse_log_file(apps->parse_log_f, app_dir, "screens 数组元素不合法", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
        }
    }
    else{
        parse_log_file(apps->parse_log_f, app_dir, "缺少字段 screens", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    if (!screen_match_this_build) {
        parse_log_file(apps->parse_log_f, app_dir, "此应用screen不兼容当前固件配置", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }


    // 解析描述
    const char* desc = json_get_string(json, "description");
    safe_strcpy(app->description, sizeof(app->description),
                desc ? desc : "(无描述)");
    


    // 解析图标（可选）
    const char* icon = json_get_string(json, "icon");
    if (icon && icon[0] != '\0') {
        char abs_icon[256];
        join_path(abs_icon, sizeof(abs_icon), app_dir, icon);
        if (file_exists_readable(abs_icon)) {
            set_lvgl_path(app->icon_path, sizeof(app->icon_path), abs_icon);
        } else {
            parse_log_file(apps->parse_log_f, app_dir, "图标文件不存在, 使用默认图标", PARSE_LOG_WARN);
            safe_strcpy(app->icon_path, sizeof(app->icon_path), APPS_DEFAULT_ICON_PATH);
        }
    } else {
        safe_strcpy(app->icon_path, sizeof(app->icon_path), APPS_DEFAULT_ICON_PATH);
    }

    // 解析拓展名
    cJSON* extensions_obj = cJSON_GetObjectItem(json, "extensions");
    if (extensions_obj && cJSON_IsArray(extensions_obj)) {
        for(int i = 0; i < cJSON_GetArraySize(extensions_obj); i++){
            cJSON* extension_obj = cJSON_GetArrayItem(extensions_obj, i);
            if (extension_obj && cJSON_IsString(extension_obj)) {
                apps_extmap_add(&apps->extmap, extension_obj->valuestring, app);
            }
        }
    }

    cJSON_Delete(json);

    return 0;
}

int apps_cfg_scan(apps_t *apps,char* dirpath,app_source_t source){
    int error_cnt = 0;
    char path[128];
    DIR *dir = opendir(dirpath);
    if (!dir) {
        parse_log_file(apps->parse_log_f, dirpath, "无法打开应用目录", PARSE_LOG_WARN);
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        if(entry->d_type != DT_DIR){
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (apps->app_count >= APPS_MAX) {
            parse_log_file(apps->parse_log_f, dirpath, "应用数量超过最大值", PARSE_LOG_WARN);
            break;
        }

        join_path(path, sizeof(path), dirpath, entry->d_name);
        if (apps_cfg_try_load(apps, &apps->apps[apps->app_count], path, source, apps->app_count) == 0) {
            apps->app_count++;
        }
        else{
            error_cnt ++;
        }
    }
    closedir(dir);
    return error_cnt;
}

#ifndef APP_RELEASE
void apps_cfg_log_entry(app_entry_t* app){
    log_debug("name: %s", app->app_name);
    log_debug("uuid: ");
    uuid_print(&app->uuid);
    log_trace("description: %s", app->description);
    log_trace("icon_path: %s", app->icon_path);
    log_trace("executable_path: %s", app->executable_path);
    log_trace("app_dir: %s", app->app_dir);
    log_trace("type: %d", app->type);
    log_trace("pid: %d", app->pid);
    log_trace("source: %d", app->source);
    log_trace("index: %d", app->index);
}
#endif // APP_RELEASE