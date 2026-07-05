#include "utils/settings.h"
#include "utils/log.h"
#include "config.h"
#include <stdlib.h>

void log_settings(settings_t *settings){
    log_info("==> Settings Log <==");
    log_info("magic: %08lx", settings->magic);
    log_info("version: %d", settings->version);
    log_info("brightness: %d", settings->brightness);
    log_info("switch_interval: %d", settings->switch_interval);
    log_info("switch_mode: %d", settings->switch_mode);
    log_info("usb_mode: %d", settings->usb_mode);
    log_info("ctrl.lowbat: %d", settings->ctrl_word.lowbat_trip);
    log_info("ctrl.no_intro: %d", settings->ctrl_word.no_intro_block);
    log_info("ctrl.no_overlay: %d", settings->ctrl_word.no_overlay_block);
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
        case usb_mode_t_EPMANAGER:
            log_info("setting usb mode to EPMANAGER");
            system("usbctl epass &");
            break;
        default:
            log_info("setting usb mode to NONE");
            system("usbctl none &");
            break;
    }

}

static void settings_save(settings_t *settings){
    FILE *f = fopen(SETTINGS_FILE_PATH, "wb");
    if (!f) {
        log_error("Failed to open settings file for writing");
        return;
    }
    settings->magic = SETTINGS_MAGIC;
    settings->version = SETTINGS_VERSION;
    if (fwrite(settings, SETTINGS_LENGTH, 1, f) != 1) {
        log_error("Failed to write settings");
    }
    fclose(f);

    log_info("setting saved!");
    // log_settings(settings);
}

void settings_init(settings_t *settings){
    FILE *f = fopen(SETTINGS_FILE_PATH, "rb");
    if(f == NULL){
        log_error("failed to open settings file");
    }
    else{
        fread(settings, SETTINGS_LENGTH, 1, f);
        if(settings->magic != SETTINGS_MAGIC){
            log_error("invalid settings file");
        }
        else if(settings->version != SETTINGS_VERSION){
            log_error("invalid settings file version");
        }
        else{
            fclose(f);
            set_brightness(settings->brightness);
            // usb_mode 已废弃：USB 由 usb_aio_handler 的 greeter 流程接管，开机不再拉 usbctl
            // log_settings(settings);
            pthread_mutex_init(&settings->mtx, NULL);
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
    settings->usb_mode = usb_mode_t_EPMANAGER;
    settings->ctrl_word.lowbat_trip = 1;
    settings->ctrl_word.no_intro_block = 0;
    settings->ctrl_word.no_overlay_block = 0;
    settings->theme_id = 0;
    pthread_mutex_init(&settings->mtx, NULL);
    settings_save(settings);
    return;
    
}

void settings_destroy(settings_t *settings){
    pthread_mutex_destroy(&settings->mtx);
}

void settings_lock(settings_t *settings){
    pthread_mutex_lock(&settings->mtx);
}
void settings_unlock(settings_t *settings){
    pthread_mutex_unlock(&settings->mtx);
}

void settings_update(settings_t *settings){
    settings_save(settings);
    set_brightness(settings->brightness);
}


