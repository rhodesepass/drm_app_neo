// UI 电池电量指示。数据来自 power_supply class 的 capacity/status，
// 两个板型底下路线不同但接口一致，见 buildroot/board/rhodesisland/epass/BATTERY.md。
#include "ui_screens/ui_services.h"
#include "ui/font_registry.h"
#include "utils/log.h"
#include "utils/compat.h"
#include "config.h"
#include "icons.h"
#include "ui_metrics.h"
#include "lvgl.h"
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/settings.h>


static lv_timer_t * g_battery_timer = NULL;
static lv_obj_t * g_battery_obj = NULL;
static int g_low_bat_count = 0;

// 读 capacity/status 会触发一次 ADC 转换，阻塞 0.13~0.26s，不能放 LVGL 线程里同步读：
// 独立线程采样，LVGL timer 只消费缓存值。
static pthread_t g_sample_thread;
static atomic_int g_sample_run = 0;
static atomic_int g_capacity = -1; // -1 = 尚无有效采样
static atomic_int g_charging = 0;

extern int g_running;
extern int g_exitcode;
extern settings_t g_settings;


static int sysfs_read_str(const char * path, char * buf, size_t size){
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
}

// 两个板型的 psy 名字不同(axp20x-battery / adc-battery)，不能写死名字，
// 只认 type == Battery 的那个目录。
static int battery_find_psy(char * dir_out, size_t size){
    DIR * d = opendir(UI_BATTERY_PSY_ROOT);
    if (d == NULL) return -1;
    struct dirent * ent;
    int found = -1;
    while (found < 0 && (ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char path[128], type[32];
        snprintf(path, sizeof(path), UI_BATTERY_PSY_ROOT "/%s/type", ent->d_name);
        if (sysfs_read_str(path, type, sizeof(type)) < 0) continue;
        if (strncmp(type, "Battery", 7) != 0) continue;
        // scope==Device 是外设电池(sim 机器上的无线触摸板之类)，系统电池
        // 是 System 或根本没有 scope 文件
        snprintf(path, sizeof(path), UI_BATTERY_PSY_ROOT "/%s/scope", ent->d_name);
        if (sysfs_read_str(path, type, sizeof(type)) == 0 && strncmp(type, "Device", 6) == 0) continue;
        snprintf(dir_out, size, UI_BATTERY_PSY_ROOT "/%s", ent->d_name);
        found = 0;
    }
    closedir(d);
    return found;
}

static void * battery_sample_thread(void * arg){
    char psy_dir[128] = "";
    char path[160], buf[32];
    int logged_missing = 0;

    while (atomic_load(&g_sample_run)) {
        if (psy_dir[0] == '\0') {
            // 驱动可能比 app 起得晚，找不到就下个周期再试
            if (battery_find_psy(psy_dir, sizeof(psy_dir)) == 0) {
                log_info("battery power_supply: %s", psy_dir);
            } else if (!logged_missing) {
                log_warn("no Battery power_supply found, keep retrying");
                logged_missing = 1;
            }
        }
        if (psy_dir[0] != '\0') {
            snprintf(path, sizeof(path), "%s/capacity", psy_dir);
            if (sysfs_read_str(path, buf, sizeof(buf)) == 0) {
                atomic_store(&g_capacity, atoi(buf));
            }
            snprintf(path, sizeof(path), "%s/status", psy_dir);
            if (sysfs_read_str(path, buf, sizeof(buf)) == 0) {
                atomic_store(&g_charging, strncmp(buf, "Charging", 8) == 0);
            }
        }
        // 化整为零地睡，destroy 时 join 不用等满一个周期
        for (int i = 0; i < UI_BATTERY_TIMER_TICK_PERIOD / 100000; i++) {
            if (!atomic_load(&g_sample_run)) break;
            usleep(100000);
        }
    }
    return NULL;
}

// 由LVGL timer回调触发。
// 别忘记：LVGL不是线程安全的。UI的读写只能由LVGL线程完成。
static void ui_battery_timer_cb(lv_timer_t * timer){
    int capacity = atomic_load(&g_capacity);
    int charging = atomic_load(&g_charging);
    if (capacity < 0) return;
    log_debug("battery: %d%% %s", capacity, charging ? "charging" : "discharging");

    char * bat_char;
    if (charging) {
        bat_char = UI_BATTERY_CHARGING_CHAR;
    }
    else if (capacity <= UI_BATTERY_EMPTY_PERCENT) {
        bat_char = UI_BATTERY_EMPTY_CHAR;
    }
    else if (capacity <= UI_BATTERY_1_4_PERCENT) {
        bat_char = UI_BATTERY_1_4_CHAR;
    }
    else if (capacity <= UI_BATTERY_1_2_PERCENT) {
        bat_char = UI_BATTERY_1_2_CHAR;
    }
    else if (capacity <= UI_BATTERY_3_4_PERCENT) {
        bat_char = UI_BATTERY_3_4_CHAR;
    }
    else {
        bat_char = UI_BATTERY_FULL_CHAR;
    }
    lv_label_set_text(g_battery_obj, bat_char);

    if (!charging && capacity <= UI_BATTERY_EMPTY_PERCENT) {
        g_low_bat_count++;
        if (g_low_bat_count == UI_BATTERY_LOW_BAT_WARNING_THRESHOLD) {
            log_warn("low battery warning");
            ui_warning(UI_WARNING_LOW_BATTERY);
        }
        if (g_low_bat_count >= UI_BATTERY_LOW_BAT_TRIP_THRESHOLD) {
            if (g_settings.ctrl_word.lowbat_trip) {
                log_error("low battery trip!");
                g_running = 0;
                g_exitcode = EXITCODE_SHUTDOWN;
            }
        }
    }
    else {
        g_low_bat_count = 0;
    }
}

void ui_battery_init(){

    atomic_store(&g_sample_run, 1);
    if (pthread_create(&g_sample_thread, NULL, battery_sample_thread, NULL) != 0) {
        log_error("failed to create battery sample thread");
        atomic_store(&g_sample_run, 0);
    }

    g_battery_timer = lv_timer_create(ui_battery_timer_cb, UI_BATTERY_TIMER_TICK_PERIOD / 1000, NULL);
    log_info("==> UI Battery Initialized!");

    g_battery_obj = lv_label_create(lv_layer_top());
    lv_obj_set_pos(g_battery_obj, UI_WIDTH - S(UI_BATTERY_PADDING) - S(UI_BATTERY_SIZE), S(UI_BATTERY_PADDING));
    lv_obj_set_style_text_font(g_battery_obj, font_get(FONT_ICON, UI_BATTERY_SIZE), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_battery_obj, lv_color_hex(0xffffffff), LV_PART_MAIN);
    lv_label_set_text(g_battery_obj, UI_BATTERY_FULL_CHAR);
    lv_obj_set_style_text_align(g_battery_obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

}
void ui_battery_destroy(){
    lv_timer_delete(g_battery_timer);
    if (atomic_load(&g_sample_run)) {
        atomic_store(&g_sample_run, 0);
        pthread_join(g_sample_thread, NULL);
    }
}
