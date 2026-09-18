#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include "utils/log.h"
#include "driver/key_enc_evdev.h"
#include "driver/input_gestures.h"
#include "ui_screens/screen_manager.h"
#include "ui_screens/screens/screen_confirm.h"
#include "ui_screens/screens/screen_oplist.h"

#define BITS(n) (((n) + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long)))
#define PULSE_MAX 64

typedef struct {
    int fd;
    dev_t device;
    char path[256];
    bool touch, buttons, dropped, contact;
    bool keys[5], blocked[5];
    int x, minimum, range;
    unsigned flags;
    power_gesture_t power;
    touch_gesture_t gesture;
} input_source_t;

typedef struct {
    input_source_t sources[KEY_ENC_EVDEV_MAX_FDS];
    uint32_t scan_at, boundary;
    bool shutdown, muted, invert, warned_legacy;
    uint32_t pulses[PULSE_MAX];
    int pulse_count;
    uint32_t last_key;
    int active_source;
    bool pressed, synthetic;
} input_backend_t;

static bool s_muted;
void key_enc_evdev_mute(bool muted) { s_muted = muted; }

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static bool bit(const unsigned long *bits, unsigned n)
{
    return !!(bits[n / (8 * sizeof(*bits))] & (1UL << (n % (8 * sizeof(*bits)))));
}

static int key_index(unsigned code)
{
    switch (code) {
        case KEY_0: return 0;
        case KEY_1: return 1;
        case KEY_2: return 2;
        case KEY_3: return 3;
        case KEY_4: return 4;
        default: return -1;
    }
}

static uint32_t nav_key(int index)
{
    static const uint32_t keys[] = { LV_KEY_END, LV_KEY_LEFT, LV_KEY_RIGHT,
                                     LV_KEY_ENTER, LV_KEY_ESC };
    return keys[index];
}

static void resync(input_source_t *s)
{
    unsigned long keys[BITS(KEY_MAX + 1)] = {0};
    ioctl(s->fd, EVIOCGKEY(sizeof(keys)), keys);
    const unsigned codes[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4};
    for (int i = 0; i < 5; ++i) s->blocked[i] = s->keys[i] = bit(keys, codes[i]);
    s->power.down = s->keys[0];
    s->power.blocked = s->power.down;
    if (s->touch) {
        struct input_absinfo abs;
        s->contact = bit(keys, BTN_TOUCH);
        if (ioctl(s->fd, EVIOCGABS(ABS_X), &abs) == 0) s->x = abs.value;
        if (ioctl(s->fd, EVIOCGABS(ABS_MISC), &abs) == 0) s->flags = abs.value;
        s->gesture.down = s->contact;
        touch_gesture_cancel(&s->gesture);
        if (!s->contact) s->gesture.blocked = false;
    }
}

static void add_source(key_enc_evdev_t *ctx, const char *path, bool explicit_key)
{
    input_backend_t *b = ctx->backend;
    int slot = -1;
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) {
        if (b->sources[i].fd >= 0 && !strcmp(b->sources[i].path, path)) return;
        if (b->sources[i].fd < 0 && slot < 0) slot = i;
    }
    if (slot < 0) return;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return; }
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) {
        if (b->sources[i].fd >= 0 && b->sources[i].device == st.st_rdev) {
            close(fd);
            return;
        }
    }
    unsigned long keys[BITS(KEY_MAX + 1)] = {0}, absbits[BITS(ABS_MAX + 1)] = {0};
    char name[128] = {0};
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits);
    bool ch32 = !strcmp(name, "CH32 touch slider");
    bool touch = ch32 && bit(keys, BTN_TOUCH) && bit(absbits, ABS_X) && bit(absbits, ABS_MISC);
    bool nav = bit(keys, KEY_0) || bit(keys, KEY_1) || bit(keys, KEY_2) ||
               bit(keys, KEY_3) || bit(keys, KEY_4);
    if ((ch32 && !touch) || (!touch && !explicit_key &&
        (!nav || (ctx->dev_path[0] && !bit(keys, KEY_0))))) {
        if (ch32 && !b->warned_legacy) {
            log_warn("CH32 touch disabled: driver lacks ABS_MISC status");
            b->warned_legacy = true;
        }
        close(fd);
        return;
    }
    input_source_t *s = &b->sources[slot];
    memset(s, 0, sizeof(*s));
    s->fd = fd;
    s->device = st.st_rdev;
    s->touch = touch;
    s->buttons = !touch && bit(keys, KEY_3);
    snprintf(s->path, sizeof(s->path), "%s", path);
    if (touch) {
        struct input_absinfo abs;
        if (ioctl(fd, EVIOCGABS(ABS_X), &abs) < 0 || abs.maximum <= abs.minimum) {
            close(fd);
            s->fd = -1;
            return;
        }
        s->minimum = abs.minimum;
        s->range = abs.maximum - abs.minimum;
    }
    int clock_id = CLOCK_MONOTONIC;
    if (ioctl(fd, EVIOCSCLOCKID, &clock_id) < 0) {
        log_warn("Input %s cannot use monotonic timestamps: %s", path, strerror(errno));
        close(fd);
        s->fd = -1;
        return;
    }
    resync(s);
    log_info("Input %s: %s (%s)", path, name, touch ? "touch" : "keys");
}

static void scan_sources(key_enc_evdev_t *ctx)
{
    input_backend_t *b = ctx->backend;
    if (ctx->dev_path[0]) add_source(ctx, ctx->dev_path, true);
    DIR *dir = opendir("/dev/input");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (strncmp(ent->d_name, "event", 5)) continue;
            char path[512];
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            add_source(ctx, path, false);
        }
        closedir(dir);
    }
    bool touch = false, buttons = false;
    ctx->evdev_fd_count = 0;
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) {
        input_source_t *s = &b->sources[i];
        if (s->fd < 0) continue;
        ctx->evdev_fds[ctx->evdev_fd_count++] = s->fd;
        touch |= s->touch;
        buttons |= s->buttons;
    }
    screen_confirm_set_input_sources(touch, buttons);
}

static void barrier(input_backend_t *b)
{
    b->pulse_count = 0;
    b->pressed = b->synthetic = false;
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) {
        input_source_t *s = &b->sources[i];
        touch_gesture_cancel(&s->gesture);
        if (!s->contact) s->gesture.blocked = false;
        for (int k = 0; k < 5; ++k) s->blocked[k] |= s->keys[k];
        if (s->power.down) s->power.blocked = true;
        s->power.pending = s->power.second = false;
    }
}

static void sync_mode(input_backend_t *b)
{
    bool shutdown = screen_confirm_is_shutdown();
    if (shutdown != b->shutdown || s_muted != b->muted) {
        barrier(b);
        b->boundary = now_ms();
        b->shutdown = shutdown;
        b->muted = s_muted;
    }
}

static void pulse(input_backend_t *b, uint32_t key)
{
    if (screens_current() == SCREEN_SPINNER) return;
    if (b->pulse_count < PULSE_MAX) b->pulses[b->pulse_count++] = key;
    else barrier(b);
}

static void action(input_backend_t *b, input_action_t a)
{
    switch (a) {
        case INPUT_POWER_MENU: screens_power_short(); break;
        case INPUT_POWER_DISPLAYIMG: screens_power_double(); break;
        case INPUT_POWER_SHUTDOWN: screens_power_long(); break;
        case INPUT_POWER_CANCEL: screen_confirm_shutdown_cancel(); break;
        case INPUT_TOUCH_DOWN_CONFIRM: screen_confirm_shutdown_proceed(); break;
        case INPUT_TOUCH_TAP: pulse(b, LV_KEY_ENTER); break;
        case INPUT_TOUCH_HOLD:
            if (screens_current() == SCREEN_SPINNER) break;
            if (screens_current() != SCREEN_OPLIST || !screen_oplist_touch_longpress())
                pulse(b, LV_KEY_ESC);
            break;
        default: break;
    }
    sync_mode(b);
}

static bool deliver(key_enc_evdev_t *ctx, lv_indev_data_t *data,
                    uint32_t key, bool down, int source, bool synthetic)
{
    input_backend_t *b = ctx->backend;
    if (synthetic && screens_current() == SCREEN_SPINNER) return false;
    if (down && ctx->input_cb) ctx->input_cb(key);
    sync_mode(b);
    if (b->shutdown || b->muted) return false;
    b->last_key = key;
    b->pressed = down;
    b->active_source = source;
    b->synthetic = synthetic && down;
    data->key = key;
    data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    return true;
}

static void key_enc_evdev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    key_enc_evdev_t *ctx = lv_indev_get_driver_data(indev);
    input_backend_t *b = ctx->backend;
    uint32_t now = now_ms();
    sync_mode(b);
    if ((uint32_t)(now - b->scan_at) >= 1000) {
        scan_sources(ctx);
        b->scan_at = now;
    }
    data->key = b->last_key;
    data->state = b->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (b->synthetic) {
        b->synthetic = b->pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) {
        input_source_t *s = &b->sources[i];
        if (s->fd < 0) continue;
        struct input_event ev;
        ssize_t n;
        while ((n = read(s->fd, &ev, sizeof(ev))) == sizeof(ev)) {
            uint32_t stamp = (uint32_t)((uint64_t)ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000);
            bool stale = b->boundary && (int32_t)(stamp - b->boundary) <= 0;
            if (ev.type == EV_SYN && ev.code == SYN_DROPPED) {
                s->dropped = true;
                barrier(b);
                continue;
            }
            if (s->dropped) {
                if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    resync(s);
                    s->dropped = false;
                }
                continue;
            }
            if (s->touch) {
                if (ev.type == EV_ABS && ev.code == ABS_X) s->x = ev.value;
                if (ev.type == EV_ABS && ev.code == ABS_MISC) s->flags = ev.value;
                if (ev.type == EV_KEY && ev.code == BTN_TOUCH) s->contact = ev.value != 0;
                if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    if (b->muted || stale) touch_gesture_cancel(&s->gesture);
                    int x = (int)((int64_t)(s->x - s->minimum) * 1023 / s->range);
                    if (x < 0) x = 0;
                    if (x > 1023) x = 1023;
                    if (b->invert) x = 1023 - x;
                    int steps;
                    input_action_t a = touch_gesture_frame(&s->gesture, s->contact, x,
                        s->flags, stamp, b->shutdown, &steps);
                    if (!b->muted) {
                        action(b, a);
                        while (steps != 0) {
                            pulse(b, steps > 0 ? LV_KEY_RIGHT : LV_KEY_LEFT);
                            steps += steps > 0 ? -1 : 1;
                        }
                    }
                }
                continue;
            }
            if (ev.type != EV_KEY || (ev.value != 0 && ev.value != 1)) continue;
            int k = key_index(ev.code);
            if (k < 0) continue;
            bool down = ev.value != 0;
            bool fresh = down && !s->keys[k];
            s->keys[k] = down;
            if (k == 0) {
                action(b, power_gesture_key(&s->power, down, stamp, b->shutdown, b->muted || stale));
                continue;
            }
            if (b->muted || stale) s->blocked[k] = down;
            if (s->blocked[k]) {
                if (!down) s->blocked[k] = false;
                continue;
            }
            if (b->shutdown) {
                if (k == 3 && fresh && !b->muted) {
                    screen_confirm_shutdown_proceed();
                    sync_mode(b);
                }
                continue;
            }
            if (b->muted || stale) continue;
            if (down && !fresh) continue;
            if (!down && (!b->pressed || b->active_source != i || b->last_key != nav_key(k))) continue;
            if (down && b->pressed) { s->blocked[k] = true; continue; }
            if (deliver(ctx, data, nav_key(k), down, i, false)) return;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
            log_warn("Input disconnected: %s", s->path);
            close(s->fd);
            s->fd = -1;
            barrier(b);
        }
        if (!b->muted) {
            uint32_t tick = now_ms();
            action(b, power_gesture_tick(&s->power, tick));
            if (s->touch) action(b, touch_gesture_tick(&s->gesture, tick));
        }
    }
    if (!b->pressed && b->pulse_count && !b->muted && !b->shutdown) {
        uint32_t key = b->pulses[0];
        memmove(b->pulses, b->pulses + 1, (size_t)--b->pulse_count * sizeof(*b->pulses));
        if (deliver(ctx, data, key, true, -1, true)) return;
    }
    data->key = b->last_key;
    data->state = b->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_indev_t *key_enc_evdev_init(key_enc_evdev_t *ctx)
{
    input_backend_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    ctx->backend = b;
    for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i) b->sources[i].fd = -1;
    const char *direction = getenv("EPASS_TOUCH_INVERT");
    b->invert = direction && !strcmp(direction, "1");
    ctx->indev = lv_indev_create();
    if (!ctx->indev) {
        free(b);
        ctx->backend = NULL;
        return NULL;
    }
    lv_indev_set_type(ctx->indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(ctx->indev, key_enc_evdev_read_cb);
    lv_indev_set_driver_data(ctx->indev, ctx);
    b->scan_at = now_ms();
    scan_sources(ctx);
    return ctx->indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t *ctx)
{
    input_backend_t *b = ctx->backend;
    if (b) {
        for (int i = 0; i < KEY_ENC_EVDEV_MAX_FDS; ++i)
            if (b->sources[i].fd >= 0) close(b->sources[i].fd);
        free(b);
        ctx->backend = NULL;
    }
    ctx->evdev_fd_count = 0;
    if (ctx->indev) lv_indev_delete(ctx->indev);
    ctx->indev = NULL;
}
