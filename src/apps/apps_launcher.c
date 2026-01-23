#include "apps/apps_launcher.h"
#include "apps/apps.h"
#include "utils/log.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

// ========== 外部全局变量 ==========

extern int g_running;
extern int g_exitcode;

// ========== 内部状态 ==========

static pid_t g_app_pid = -1;
static app_exit_callback_t g_exit_callback = NULL;
static void* g_callback_userdata = NULL;

// ========== 公共接口 ==========

/**
 * @brief 启动应用
 *
 * 使用退出-重启方案：
 * 1. 将应用路径写入 /tmp/appstart
 * 2. 设置 g_exitcode = EXITCODE_APPSTART
 * 3. 设置 g_running = 0 使主程序退出
 * 4. 外部脚本负责执行应用并在应用退出后重启主程序
 */
int app_launch(apps_manager_t* mgr, int app_index) {
    if (!mgr) return -1;

    app_entry_t* app = apps_get(mgr, app_index);
    if (!app) {
        log_error("[LAUNCHER] Invalid app index: %d", app_index);
        return -1;
    }

    log_info("[LAUNCHER] Launching app: %s (%s)", app->name, app->exec_path);

    // 检查可执行文件是否存在
    if (access(app->exec_path, F_OK) != 0) {
        log_error("[LAUNCHER] Executable not found: %s", app->exec_path);
        return -1;
    }

    // 检查可执行文件权限，必要时添加执行权限
    if (access(app->exec_path, X_OK) != 0) {
        if (chmod(app->exec_path, 0755) != 0) {
            log_error("[LAUNCHER] Cannot make executable: %s (%s)",
                      app->exec_path, strerror(errno));
            return -1;
        }
        log_info("[LAUNCHER] Added execute permission to: %s", app->exec_path);
    }

    // 写入启动信息到 /tmp/appstart
    FILE* f = fopen("/tmp/appstart", "w");
    if (!f) {
        log_error("[LAUNCHER] Failed to create /tmp/appstart: %s", strerror(errno));
        return -1;
    }

    // 写入启动脚本内容
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "cd \"%s\"\n", app->app_dir);
    fprintf(f, "exec \"%s\"\n", app->exec_path);
    fclose(f);

    // 设置脚本可执行
    chmod("/tmp/appstart", 0755);

    log_info("[LAUNCHER] App launch script written to /tmp/appstart");

    // 更新状态
    g_app_pid = -1;  // 还没有实际的 PID
    app->pid = -1;
    mgr->running_index = app_index;
    mgr->state = APP_STATE_RUNNING;

    // 通知主循环退出，使用 EXITCODE_APPSTART
    g_running = 0;
    g_exitcode = EXITCODE_APPSTART;

    log_info("[LAUNCHER] Requesting app launch via exit code %d", EXITCODE_APPSTART);
    return 0;
}

int app_launch_async(apps_manager_t* mgr, int app_index,
                     app_exit_callback_t callback, void* userdata) {
    // 保存回调（实际上在退出-重启方案中不会调用）
    g_exit_callback = callback;
    g_callback_userdata = userdata;

    return app_launch(mgr, app_index);
}

int app_check_exit(apps_manager_t* mgr) {
    // 在退出-重启方案中，这个函数不会被调用
    // 因为主程序会在 app_launch 后退出
    (void)mgr;
    return -1;
}

int app_kill(apps_manager_t* mgr) {
    // 在退出-重启方案中，这个函数不会被调用
    (void)mgr;
    return -1;
}
