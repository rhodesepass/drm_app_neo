#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { TOUCH_VALID = 1, TOUCH_MULTI = 2, TOUCH_CANCEL = 4 };
enum {
    POWER_HOLD_MS = 1500,
    POWER_DOUBLE_MS = 350,
    TOUCH_HOLD_MS = 600,
    TOUCH_TAP_MIN_MS = 30,
    TOUCH_TAP_MAX_MS = 350,
    TOUCH_MOVE_SLOP = 32,
    TOUCH_NAV_STEP = 160,
    TOUCH_CONFIRM_DISTANCE = 256,
    TOUCH_CONFIRM_REVERSE = 32
};
typedef enum {
    INPUT_NONE, INPUT_POWER_MENU, INPUT_POWER_SHUTDOWN, INPUT_POWER_CANCEL,
    INPUT_POWER_DISPLAYIMG,
    INPUT_TOUCH_TAP, INPUT_TOUCH_HOLD, INPUT_TOUCH_DOWN_CONFIRM
} input_action_t;

typedef struct {
    bool down, blocked, fired, pending, second;
    uint32_t started, released;
} power_gesture_t;

typedef struct {
    bool down, blocked, moved, held, shutdown;
    int start, last, remainder, max_offset, reverse;
    uint32_t started;
} touch_gesture_t;

input_action_t power_gesture_key(power_gesture_t *s, bool down, uint32_t now,
                                 bool shutdown, bool muted);
input_action_t power_gesture_tick(power_gesture_t *s, uint32_t now);
void touch_gesture_cancel(touch_gesture_t *s);
input_action_t touch_gesture_frame(touch_gesture_t *s, bool down, int x,
                                   unsigned flags, uint32_t now, bool shutdown,
                                   int *steps);
input_action_t touch_gesture_tick(touch_gesture_t *s, uint32_t now);
