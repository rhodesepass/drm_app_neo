#include "utils/settings.h"
#include "utils/log.h"
#include "config.h"
#include <stdlib.h>

static void log_settings(settings_t *settings){
    log_info("==========> Settings Log <==========");
    log_info("magic: %08lx", settings->magic);
    log_info("version: %d", settings->version);
    log_info("brightness: %d", settings->brightness);
    log_info("switch_interval: %d", settings->switch_interval);
    log_info("switch_mode: %d", settings->switch_mode);
    log_info("usb_mode: %d", settings->usb_mode);
    log_info("ctrl_word.lowbat_trip: %d", settings->ctrl_word.lowbat_trip);
    log_info("ctrl_word.no_intro_block: %d", settings->ctrl_word.no_intro_block);
    log_info("ctrl_word.no_overlay_block: %d", settings->ctrl_word.no_overlay_block);
}

static void set_brightness(int brightness){
    FILE *f = fopen(SETTINGS_BRIGHTNESS_PATH, "w");
    if (f) {
        fprintf(f, "%d\n", brightness);
        fclose(f);
    } else {
        log_error("Failed to set brightness");
    }
}

void settings_set_usb_mode(usb_mode_t usb_mode){
    switch(usb_mode){
        case usb_mode_t_MTP:
            log_info("setting usb mode to MTP");
            system("usbctl mtp &");
            break;
        case usb_mode_t_SERIAL:
            log_info("setting usb mode to SERIAL");
            system("usbctl serial &");
            break;
        case usb_mode_t_RNDIS:
            log_info("setting usb mode to RNDIS");
            system("usbctl rndis &");
            break;
        default:
            log_info("setting usb mode to NONE");
            system("usbctl none &");
            break;
    }

}

static void settings_save(settings_t *settings){
    FILE *f = fopen(SETTINGS_FILE_PATH, "wb");
    settings->magic = SETTINGS_MAGIC;
    settings->version = SETTINGS_VERSION;
    fwrite(settings, sizeof(settings_t), 1, f);
    fclose(f);

    log_info("setting saved!");
    log_settings(settings);
}

void settings_init(settings_t *settings){
    FILE *f = fopen(SETTINGS_FILE_PATH, "rb");
    if(f == NULL){
        log_error("failed to open settings file");
    }
    else{
        fread(settings, sizeof(settings_t), 1, f);
        if(settings->magic != SETTINGS_MAGIC){
            log_error("invalid settings file");
        }
        else if(settings->version != SETTINGS_VERSION){
            log_error("invalid settings file version");
        }
        else{
            fclose(f);
            set_brightness(settings->brightness);
            settings_set_usb_mode(settings->usb_mode);
            log_settings(settings);
            return;
        }
        fclose(f);
    }

    log_info("creating new settings file");
    settings->magic = SETTINGS_MAGIC;
    settings->version = SETTINGS_VERSION;
    settings->brightness = 5;
    settings->switch_interval = sw_interval_t_SW_INTERVAL_5MIN;
    settings->switch_mode = sw_mode_t_SW_MODE_SEQUENCE;
    settings->usb_mode = usb_mode_t_MTP;
    settings->ctrl_word.lowbat_trip = 1;
    settings->ctrl_word.no_intro_block = 0;
    settings->ctrl_word.no_overlay_block = 0;
    settings_set_usb_mode(settings->usb_mode);
    settings_save(settings);
    return;
    
}


void settings_update(settings_t *settings){
    settings_save(settings);
    set_brightness(settings->brightness);
}


