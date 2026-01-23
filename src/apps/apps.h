/**
 * @file apps.h
 * @brief 用户应用管理模块 - 数据结构定义
 *
 * 提供用户应用的扫描、加载和管理功能。
 * 应用配置使用 appconfig.json 文件定义。
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 常量定义 ==========

#define APPS_MAX_COUNT          64      // 最大应用数量
#define APP_NAME_MAX_LEN        64      // 应用名称最大长度
#define APP_DESC_MAX_LEN        256     // 应用描述最大长度
#define APP_PATH_MAX_LEN        256     // 路径最大长度
#define APP_VERSION_MAX_LEN     32      // 版本号最大长度

#define APPS_CONFIG_FILENAME    "appconfig.json"
#define APPS_CONFIG_VERSION     1

#define APPS_DIR_NAND           "/apps/"
#define APPS_DIR_SD             "/sd/apps/"
#define APPS_PARSE_LOG_PATH     "/root/apps.log"
#define APPS_DEFAULT_ICON_PATH  "A:/root/res/default_app_icon.png"

#define APP_ICON_SIZE           48      // 图标尺寸 48x48

// ========== 枚举定义 ==========

/**
 * @brief 应用状态
 */
typedef enum {
    APP_STATE_IDLE = 0,         // 空闲，未运行
    APP_STATE_RUNNING,          // 正在运行
    APP_STATE_STOPPED           // 已停止
} app_state_t;

/**
 * @brief 应用来源
 */
typedef enum {
    APP_SOURCE_NAND = 0,        // 来自 NAND 闪存
    APP_SOURCE_SD               // 来自 SD 卡
} app_source_t;

/**
 * @brief 日志类型
 */
typedef enum {
    APP_LOG_ERROR = 0,
    APP_LOG_WARN,
    APP_LOG_INFO
} app_log_type_t;

// ========== 数据结构 ==========

/**
 * @brief 应用条目结构
 */
typedef struct {
    int index;                              // 在数组中的索引

    // 基本信息
    char name[APP_NAME_MAX_LEN];            // 应用名称
    char description[APP_DESC_MAX_LEN];     // 应用描述
    char author[APP_NAME_MAX_LEN];          // 作者
    char app_version[APP_VERSION_MAX_LEN];  // 应用版本

    // 路径信息
    char app_dir[APP_PATH_MAX_LEN];         // 应用目录路径
    char exec_path[APP_PATH_MAX_LEN];       // 可执行文件完整路径
    char icon_path[APP_PATH_MAX_LEN];       // 图标路径 (LVGL格式: A:/...)

    // 元数据
    app_source_t source;                    // 应用来源 (NAND/SD)

    // 运行时状态
    pid_t pid;                              // 运行时进程ID，-1表示未运行

} app_entry_t;

/**
 * @brief 应用管理器结构
 */
typedef struct {
    app_entry_t apps[APPS_MAX_COUNT];       // 应用数组
    int count;                              // 当前应用数量

    int running_index;                      // 当前运行的应用索引，-1表示无
    app_state_t state;                      // 管理器状态

    FILE* parse_log;                        // 解析日志文件

} apps_manager_t;

// ========== 函数声明 ==========

/**
 * @brief 初始化应用管理器
 * @param mgr 管理器指针
 * @return 0成功，-1失败
 */
int apps_manager_init(apps_manager_t* mgr);

/**
 * @brief 销毁应用管理器
 * @param mgr 管理器指针
 */
void apps_manager_destroy(apps_manager_t* mgr);

/**
 * @brief 扫描应用目录
 * @param mgr 管理器指针
 * @param use_sd 是否扫描SD卡
 * @return 找到的应用数量
 */
int apps_scan(apps_manager_t* mgr, bool use_sd);

/**
 * @brief 获取应用条目
 * @param mgr 管理器指针
 * @param index 应用索引
 * @return 应用指针，失败返回NULL
 */
app_entry_t* apps_get(apps_manager_t* mgr, int index);

/**
 * @brief 获取应用数量
 * @param mgr 管理器指针
 * @return 应用数量
 */
int apps_get_count(apps_manager_t* mgr);

/**
 * @brief 检查是否有应用正在运行
 * @param mgr 管理器指针
 * @return true正在运行，false未运行
 */
bool apps_is_running(apps_manager_t* mgr);

/**
 * @brief 记录解析日志
 * @param mgr 管理器指针
 * @param path 应用路径
 * @param message 日志消息
 * @param type 日志类型
 */
void apps_log(apps_manager_t* mgr, const char* path,
              const char* message, app_log_type_t type);

#ifdef __cplusplus
}
#endif
