// UI 电池电量指示
#include "ui.h"
#include "ui/actions_warning.h"
#include "utils/log.h"
#include "ui/scr_transition.h"
#include "config.h"
#include "lvgl.h"
#include <src/font/lv_font.h>
#include <src/misc/lv_color.h>
#include <src/widgets/label/lv_label.h>
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

LV_FONT_DECLARE(ui_font_fontawesome);

void ui_battery_init(){

    g_battery_timer = lv_timer_create(ui_battery_timer_cb, UI_BATTERY_TIMER_TICK_PERIOD / 1000, NULL);
    log_info("==> UI Battery Initialized!");

    g_battery_obj = lv_label_create(lv_layer_top());
    lv_obj_set_pos(g_battery_obj, UI_WIDTH-UI_BATTERY_PADDING-UI_BATTERY_SIZE, UI_BATTERY_PADDING);
    // lv_obj_set_size(g_battery_obj, UI_BATTERY_SIZE, UI_BATTERY_SIZE);
    lv_obj_set_style_text_font(g_battery_obj, &ui_font_fontawesome, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_battery_obj, lv_color_hex(0xffffffff), LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(g_battery_obj, 100, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(g_battery_obj, 100, LV_PART_MAIN);
    lv_label_set_text(g_battery_obj, UI_BATTERY_FULL_CHAR);
    lv_obj_set_style_text_align(g_battery_obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

}
void ui_battery_destroy(){
    lv_timer_delete(g_battery_timer);
}
