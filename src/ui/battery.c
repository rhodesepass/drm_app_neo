// UI 电池电量指示
#include "ui_screens/ui_services.h"
#include "ui/font_registry.h"
#include "utils/log.h"
#include "utils/compat.h"
#include "config.h"
#include "icons.h"
#include "ui_metrics.h"
#include "lvgl.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <utils/settings.h>


static lv_timer_t * g_battery_timer = NULL;
static lv_obj_t * g_battery_obj = NULL;
static int g_low_bat_count = 0;

extern int g_running;
extern int g_exitcode;
extern settings_t g_settings;

// =========================================
// 自己添加的方法 START
// =========================================


// 由LVGL timer回调触发。
// 别忘记：LVGL不是线程安全的。UI的读写只能由LVGL线程完成。
static void ui_battery_timer_cb(lv_timer_t * timer){
    int fd;
    char buf[32];
    int value;
    fd = open(UI_BATTERY_ADC_PATH, O_RDONLY | O_SYNC);
    if (fd < 0) {
        log_error("failed to open battery adc file");
        return;
    }
    read(fd, buf, sizeof(buf));
    close(fd);
    value = atoi(buf);
    log_debug("battery value: %d", value);

    char* bat_char;
    if(value < UI_BATTERY_EMPTY_VALUE){
        bat_char = UI_BATTERY_EMPTY_CHAR;
    }
    else if(value < UI_BATTERY_1_4_VALUE){
        bat_char = UI_BATTERY_1_4_CHAR;
    }
    else if(value < UI_BATTERY_1_2_VALUE){
        bat_char = UI_BATTERY_1_2_CHAR;
    }
    else if(value < UI_BATTERY_3_4_VALUE){
        bat_char = UI_BATTERY_3_4_CHAR;
    }
    else if(value > UI_BATTERY_CHARGING_VALUE){
        bat_char = UI_BATTERY_CHARGING_CHAR;
    }
    else{
        bat_char = UI_BATTERY_FULL_CHAR;
    }
    lv_label_set_text(g_battery_obj, bat_char);

    if(value < UI_BATTERY_EMPTY_VALUE){
        g_low_bat_count++;
        if(g_low_bat_count == UI_BATTERY_LOW_BAT_WARNING_THRESHOLD){
            log_warn("low battery warning");
            ui_warning(UI_WARNING_LOW_BATTERY);
        }
        if(g_low_bat_count >= UI_BATTERY_LOW_BAT_TRIP_THRESHOLD){
            if(g_settings.ctrl_word.lowbat_trip){
                log_error("low battery trip!");
                g_running = 0;
                g_exitcode = EXITCODE_SHUTDOWN;
            }
        }
    }
    else{
        g_low_bat_count = 0;
    }

}

void ui_battery_init(){

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
}
