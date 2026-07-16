#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <limits.h>

#include "utils/log.h"
#include "lvgl.h"
#include "driver/key_enc_evdev.h"


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
        case KEY_0:
            return LV_KEY_END;
        default:
            return 0;
    }
}

void print_lv_key_str(int key){
    switch(key){
        case LV_KEY_LEFT:
            log_debug("LV_KEY_LEFT");
            break;
        case LV_KEY_RIGHT:
            log_debug("LV_KEY_RIGHT");
            break;
        case LV_KEY_ENTER:
            log_debug("LV_KEY_ENTER");
            break;
        case LV_KEY_ESC:
            log_debug("LV_KEY_ESC");
            break;
        case LV_KEY_END:
            log_debug("LV_KEY_END");
            break;
        default:
            log_debug("unknown key: %d", key);
    }
}

static int key_bit_test(const unsigned long *bits, int bit)
{
    return !!(bits[bit / (sizeof(unsigned long) * 8)] &
              (1UL << (bit % (sizeof(unsigned long) * 8))));
}

/* 设备是否具备我们关心的数字键（过滤触摸屏等纯 BTN_* 设备） */
static int key_enc_evdev_has_nav_keys(int fd)
{
    unsigned long key_bits[(KEY_MAX + 1 + (sizeof(unsigned long) * 8) - 1) /
                           (sizeof(unsigned long) * 8)];
    memset(key_bits, 0, sizeof(key_bits));
    if(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return 0;
    }
    return key_bit_test(key_bits, KEY_0) ||
           key_bit_test(key_bits, KEY_1) ||
           key_bit_test(key_bits, KEY_2) ||
           key_bit_test(key_bits, KEY_3) ||
           key_bit_test(key_bits, KEY_4);
}

static int key_enc_evdev_open_one(const char *path, int require_nav_keys)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        log_warn("Failed to open evdev %s: %s", path, strerror(errno));
        return -1;
    }
    if(require_nav_keys && !key_enc_evdev_has_nav_keys(fd)) {
        close(fd);
        return -1;
    }
    log_info("Opened key evdev: %s (fd=%d)", path, fd);
    return fd;
}

static void key_enc_evdev_add_fd(key_enc_evdev_t *ctx, int fd)
{
    if(fd < 0) {
        return;
    }
    if(ctx->evdev_fd_count >= KEY_ENC_EVDEV_MAX_FDS) {
        log_warn("Too many evdev devices (max %d), skipping fd %d",
                 KEY_ENC_EVDEV_MAX_FDS, fd);
        close(fd);
        return;
    }
    ctx->evdev_fds[ctx->evdev_fd_count++] = fd;
}

static void key_enc_evdev_scan_all(key_enc_evdev_t *ctx)
{
    DIR *dir = opendir("/dev/input");
    if(!dir) {
        log_error("Failed to open /dev/input: %s", strerror(errno));
        return;
    }

    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
        /* 只认 eventN，跳过 mice / js* 等 */
        if(strncmp(ent->d_name, "event", 5) != 0) {
            continue;
        }
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = key_enc_evdev_open_one(path, 1);
        key_enc_evdev_add_fd(ctx, fd);
    }
    closedir(dir);

    log_info("key_enc_evdev: scanned %d event device(s) with nav keys",
             ctx->evdev_fd_count);
}

// 官方文档中说只需要在key_pressed的时候回传data->key
// 实际上松开的事件也需要回传，否则默认的事件是“放开了enter按钮”，导致误触发选择。
// 此外 evdev只会在按钮按下/松开的时候产生数据（被read），因此需要缓存数据。
static void key_enc_evdev_read_cb(lv_indev_t * indev, lv_indev_data_t * data){

    static int last_key = 0;
    static int last_state = LV_INDEV_STATE_RELEASED;

    key_enc_evdev_t * key_enc_evdev = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);
    struct input_event in = { 0 };

    for(int i = 0; i < key_enc_evdev->evdev_fd_count; i++) {
        int fd = key_enc_evdev->evdev_fds[i];
        while(1){
            int bytes_read = read(fd, &in, sizeof(in));
            if(bytes_read <= 0){
                break; /* 该 fd 暂无数据，试下一个 */
            }
            if(in.type != EV_KEY){
                continue;
            }

            if(in.value == 1) {
                int key = key_enc_evdev_process_key(in.code);
                if(key == 0) {
                    continue; /* 非导航键，忽略 */
                }
                data->key = key;
                data->state = LV_INDEV_STATE_PRESSED;
                if(key_enc_evdev->input_cb) {
                    key_enc_evdev->input_cb(data->key);
                }
                last_key = data->key;
                last_state = data->state;
                return;
            } else if(in.value == 0) {
                int key = key_enc_evdev_process_key(in.code);
                if(key == 0) {
                    continue;
                }
                data->state = LV_INDEV_STATE_RELEASED;
                data->key = last_key;
                last_state = data->state;
                return;
            }
            /* value==2: 自动重复，忽略 */
        }
    }

    data->key = last_key;
    data->state = last_state;
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, key_enc_evdev_read_cb);
    key_enc_evdev->indev = indev;
    key_enc_evdev->evdev_fd_count = 0;
    lv_indev_set_driver_data(indev, key_enc_evdev);

    if(key_enc_evdev->dev_path[0] != '\0') {
        /* 指定路径：不要求 ioctl 过滤（兼容旧用法） */
        int fd = key_enc_evdev_open_one(key_enc_evdev->dev_path, 0);
        if(fd < 0) {
            log_error("Failed to open evdev file: %s", key_enc_evdev->dev_path);
            return NULL;
        }
        key_enc_evdev_add_fd(key_enc_evdev, fd);
    } else {
        key_enc_evdev_scan_all(key_enc_evdev);
        if(key_enc_evdev->evdev_fd_count == 0) {
            log_error("No /dev/input/event* with nav keys found");
            return NULL;
        }
    }

    return indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev){
    for(int i = 0; i < key_enc_evdev->evdev_fd_count; i++) {
        if(key_enc_evdev->evdev_fds[i] >= 0) {
            close(key_enc_evdev->evdev_fds[i]);
            key_enc_evdev->evdev_fds[i] = -1;
        }
    }
    key_enc_evdev->evdev_fd_count = 0;
    lv_indev_delete(key_enc_evdev->indev);
}
