#include "apps.h"
#include "utils/log.h"
#include "utils/timer.h"
#include <apps/apps_cfg_parse.h>
#include <apps/extmap.h>
#include <config.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ui/actions_warning.h>
#include <unistd.h>
#include <utils/misc.h>
#include "apps/ipc_common.h"
#include "apps/ipc_server.h"

static void apps_bg_app_check_timer_cb(void *userdata, bool is_last) {
    apps_t *apps = (apps_t *)userdata;
    for (int i = 0; i < apps->app_count; i++) {
        if (apps->apps[i].type == APP_TYPE_BACKGROUND) {
        if (apps->apps[i].pid != -1) {
            // if pid still alive
            if (kill(apps->apps[i].pid, 0) == 0) {
            continue;
            } else {
            // pid is not alive
            apps->apps[i].pid = -1;
            }
        }
        }
    }
}

int apps_init(apps_t *apps, prts_t *prts, bool use_sd) {
    log_info("==> Apps Initializing...");
    apps->app_count = 0;
    apps->prts = prts;

    apps->parse_log_f = fopen(APPS_PARSE_LOG, "w");
    if (apps->parse_log_f == NULL) {
        log_error("failed to open parse log file: %s", APPS_PARSE_LOG);
    }

    int errcnt = apps_cfg_scan(apps, APPS_DIR, APP_SOURCE_NAND);

    if (use_sd) {
        log_info("==> Apps will scan SD directory: %s", APPS_DIR_SD);
        errcnt += apps_cfg_scan(apps, APPS_DIR_SD, APP_SOURCE_SD);
    }

    if (errcnt != 0) {
        log_warn("failed to load apps, error count: %d", errcnt);
        ui_warning(UI_WARNING_APP_LOAD_ERROR);
    }

#ifndef APP_RELEASE
    for (int i = 0; i < apps->app_count; i++) {
        apps_cfg_log_entry(&apps->apps[i]);
    }
    apps_extmap_log_entry(&apps->extmap);
#endif // APP_RELEASE

    prts_timer_create(&apps->bg_app_check_timer, 0, APPS_BG_APP_CHECK_PERIOD, -1,
                        apps_bg_app_check_timer_cb, apps);

    atomic_store(&apps->ipc_running, 1);
    pthread_create(&apps->ipc_thread, NULL, apps_ipc_server_thread, apps);

    log_info("==> Apps Initalized! %d apps loaded", apps->app_count);
    return 0;
}

static int kill_app_background(int pgid);

int apps_destroy(apps_t *apps) {
    prts_timer_cancel(apps->bg_app_check_timer);

    //kill all background apps
    for (int i = 0; i < apps->app_count; i++) {
        if (apps->apps[i].type == APP_TYPE_BACKGROUND && apps->apps[i].pid != -1) {
            kill_app_background(apps->apps[i].pid);
        }
    }
    pid_t pid;
    int status;
    usleep(1 * 1000 * 1000);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            log_info("destory:child process %d exited with status %d", pid, WEXITSTATUS(status));
        }
    }

    atomic_store(&apps->ipc_running, 0);
    pthread_join(apps->ipc_thread, NULL);
    fclose(apps->parse_log_f);
    return 0;
}

extern int g_exitcode;
extern int g_running;

static int launch_app_foreground(const char *basename, const char *working_dir,
                                 const char *args) {
    if (!basename || !working_dir)
        return -1;

    char exec[128];
    join_path(exec, sizeof(exec), working_dir, basename);
    if (!file_exists_readable(exec)) {
        log_error("executable file not found: %s", exec);
        return -1;
    }

    FILE *fp = fopen("/tmp/appstart", "w");
    if (!fp) {
        log_error("unable to write to /tmp/appstart???: %s", strerror(errno));
        return -1;
    }

    fprintf(fp, "#!/bin/sh\n");
    fprintf(fp, "chmod +x %s\n", exec);
    fprintf(fp, "cd %s\n", working_dir);
    if (args && args[0] != '\0') {
        fprintf(fp, "%s %s\n", exec, args);
    } else {
        fprintf(fp, "%s\n", exec);
    }

    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    log_info("/tmp/appstart written: %s %s %s", exec, working_dir, args);
    g_exitcode = EXITCODE_APPSTART;
    g_running = 0;
    return 0;
}

// return pid/pgid if success, -1 if failed
static int launch_app_background(const char *basename, const char *working_dir,
                                 const char *arg_path) // 单个参数：文件路径
{
    if (!basename || !working_dir)
        return -1;

    char exec_path[128];
    join_path(exec_path, sizeof(exec_path), working_dir, basename);
    if (!file_exists_readable(exec_path))
        return -1;

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        if (setpgid(0, 0) != 0)
            _exit(127);

        if (chdir(working_dir) != 0)
            _exit(127);

        if (arg_path && arg_path[0] != '\0')
            execl(exec_path, basename, arg_path, (char *)NULL);
        else
            execl(exec_path, basename, (char *)NULL);

        _exit(127);
    }

    (void)setpgid(pid, pid);

    log_info("launched app background: %s,pid: %d", exec_path, pid);
    return pid; // 这个 pid，同时它也是 pgid
}

static int kill_app_background(int pgid) {
    if (pgid <= 0)
        return -1;

    // 先温和退出
    kill(-pgid, SIGTERM);

    const int step_us = 50000;
    for (int waited = 0; waited < APPS_BG_KILL_TIMEOUT_US; waited += step_us) {
        if (kill(-pgid, 0) == -1 && errno == ESRCH)
            return 0; // 组里已无人
        usleep(step_us);
    }

    log_warn("app background %d did not exit, killing it forcefully", pgid);
    // 还没退出就强杀
    kill(-pgid, SIGKILL);
    return 0;
}

int apps_try_launch_by_file(apps_t *apps, const char *working_dir,
                            const char *basename) {
    if (!working_dir || !basename)
        return -1;

    char ext[10];
    // 提取文件扩展名
    const char *dot = strrchr(basename, '.');
    if (dot && *(dot + 1) != '\0') {
        strncpy(ext, dot, sizeof(ext) - 1);
        ext[sizeof(ext) - 1] = '\0';
    } else {
        ext[0] = '\0';
    }

    if (ext[0] == '\0') {
        log_info("no extension found, launch app foreground: %s", basename);
        return launch_app_foreground(basename, working_dir, NULL);
    }

    app_entry_t *app = NULL;
    apps_extmap_get(&apps->extmap, ext, &app);
    if (!app) {
        log_info("no extension found, launch app foreground: %s", basename);
        return launch_app_foreground(basename, working_dir, NULL);
    }

    const char *app_basename = path_basename(app->executable_path);
    if (!app_basename) {
        log_error("unable to get basename of %s", app->executable_path);
        return -1;
    }
    char file_full_path[128];
    join_path(file_full_path, sizeof(file_full_path), working_dir, basename);
    if (!file_exists_readable(file_full_path)) {
        log_error("file not found: %s", file_full_path);
        return -1;
    }

    if (app->type == APP_TYPE_BACKGROUND) {
        log_info("launch app foreground: %s,file: %s", app->executable_path,
                file_full_path);

        if(app->pid != -1){
            ui_warning(UI_WARNING_APP_ALREADY_RUNNING);
            return -1;
        }
        
        int pid = launch_app_background(app_basename, app->app_dir, file_full_path);
        if (pid > 0) {
            app->pid = pid;
            return 0;
        } 
        else {
            log_error("failed to launch app background: %s", app->executable_path);
            return -1;
        }
    }
    log_info("launch app foreground: %s,file: %s", app->executable_path,
            file_full_path);
    return launch_app_foreground(app_basename, app->app_dir, file_full_path);
}

int apps_try_launch_by_index(apps_t *apps, int index) {
    if (index < 0 || index >= apps->app_count)
        return -1;
    app_entry_t *app = &apps->apps[index];
    if (app->type == APP_TYPE_FOREGROUND_EXTENSION_ONLY) {
        return -1;
    } else if (app->type == APP_TYPE_FOREGROUND) {
        return launch_app_foreground(app->executable_path, app->app_dir, NULL);
    }
    return -1;
}

int apps_toggle_bg_app_by_index(apps_t *apps, int index) {
    if (index < 0 || index >= apps->app_count)
        return -1;
    app_entry_t *app = &apps->apps[index];
    if (app->type != APP_TYPE_BACKGROUND)
        return -1;
    if (app->pid == -1) {
        int pid = launch_app_background(app->executable_path, app->app_dir, NULL);
        if (pid > 0) {
            app->pid = pid;
            return 0;
        } else {
            log_error("failed to launch app background: %s", app->executable_path);
            return -1;
        }
    } else {
        return kill_app_background(app->pid);
    }
}