// key as encoder with evdev support

#include "key_enc_evdev.h"
#include "stdint.h"
#include "lvgl.h"
#include "unistd.h"
#include <fcntl.h>
#include "log.h"
#include <linux/input.h>
#include <errno.h>


static int key_enc_evdev_process_key(uint16_t code)
{
    switch(code) {
        case KEY_1:
            return LV_KEY_LEFT;
        case KEY_2:
            return LV_KEY_RIGHT;
        case KEY_3:
            return LV_KEY_ENTER;
        case KEY_4:
            return LV_KEY_ESC;
        default:
            return 0;
    }
}

static void key_enc_evdev_read_cb(lv_indev_t * indev, lv_indev_data_t * data){

    key_enc_evdev_t * key_enc_evdev = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);
    struct input_event in = { 0 };
    // while(1){
        int bytes_read = read(key_enc_evdev->evdev_fd, &in, sizeof(in));
        // log_debug("key_enc_evdev_read_cb: bytes_read = %d, type = %d, code = %d, value = %d", bytes_read, in.type, in.code, in.value);
        if(bytes_read <= 0){
            return;
        }
        if(in.type != EV_KEY){
            return;
        }
        if(in.code != KEY_1 && in.code != KEY_2 && in.code != KEY_3 && in.code != KEY_4){
            return;
        }

        if(in.value == 1) {
            /* Get the last pressed or released key
            * use LV_KEY_ENTER for encoder press */
            data->key = key_enc_evdev_process_key(in.code);
            data->state = LV_INDEV_STATE_PRESSED;
            // data->continue_reading = true;
            key_enc_evdev->input_cb(data->key);
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    // }
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, key_enc_evdev_read_cb);
    key_enc_evdev->indev = indev;
    lv_indev_set_driver_data(indev, key_enc_evdev);

    key_enc_evdev->evdev_fd = open(key_enc_evdev->dev_path, O_RDONLY);
    if(key_enc_evdev->evdev_fd < 0){
        log_error("Failed to open evdev file: %s", key_enc_evdev->dev_path);
        return NULL;
    }

    if(fcntl(key_enc_evdev->evdev_fd, F_SETFL, O_NONBLOCK) < 0) {
        log_error("fcntl failed: %d", errno);
        return NULL;
    }

    return indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev){
    close(key_enc_evdev->evdev_fd);
    lv_indev_delete(key_enc_evdev->indev);
}