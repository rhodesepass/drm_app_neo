#pragma once

#include "apps/apps.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动应用回调函数类型
 * @param exit_code 应用退出码
 * @param userdata 用户数据
 */
typedef void (*app_exit_callback_t)(int exit_code, void* userdata);

/**
 * @brief 启动应用 (DRM完全释放方案)
 *
 * 此函数会：
 * 1. 停止 LVGL 渲染线程
 * 2. 完全释放 DRM 资源
 * 3. fork 并 exec 用户应用
 * 4. 等待应用退出 (阻塞)
 * 5. 重新初始化 DRM 和 LVGL
 *
 * @param mgr 应用管理器指针
 * @param app_index 要启动的应用索引
 * @return 0成功，-1失败
 */
int app_launch(apps_manager_t* mgr, int app_index);

/**
 * @brief 启动应用 (异步版本，带回调)
 *
 * @param mgr 应用管理器指针
 * @param app_index 要启动的应用索引
 * @param callback 应用退出时的回调函数
 * @param userdata 传递给回调的用户数据
 * @return 0成功启动，-1失败
 */
int app_launch_async(apps_manager_t* mgr, int app_index,
                     app_exit_callback_t callback, void* userdata);

/**
 * @brief 检查应用是否已退出
 *
 * 用于异步启动模式，在主循环中调用。
 *
 * @param mgr 应用管理器指针
 * @return 1 应用已退出，0 仍在运行，-1 无应用运行
 */
int app_check_exit(apps_manager_t* mgr);

/**
 * @brief 强制终止正在运行的应用
 *
 * @param mgr 应用管理器指针
 * @return 0成功，-1失败
 */
int app_kill(apps_manager_t* mgr);

#ifdef __cplusplus
}
#endif
