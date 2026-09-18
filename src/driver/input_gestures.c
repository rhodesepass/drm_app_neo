#include "input_gestures.h"
#include <stdlib.h>

input_action_t power_gesture_key(power_gesture_t *s, bool down, uint32_t now,
                                 bool shutdown, bool muted)
{
    if (down == s->down) return INPUT_NONE;
    s->down = down;
    if (down) {
        s->started = now;
        s->fired = false;
        s->blocked = muted || shutdown;
        if (s->blocked) {
            s->pending = s->second = false;
        } else {
            s->second = s->pending && (uint32_t)(now - s->released) < POWER_DOUBLE_MS;
            bool expired = s->pending && !s->second;
            s->pending = false;
            if (expired) {
                s->blocked = true;
                return INPUT_POWER_MENU;
            }
        }
        return shutdown && !muted ? INPUT_POWER_CANCEL : INPUT_NONE;
    }
    input_action_t result = INPUT_NONE;
    if (!s->blocked && !s->fired && !muted && !shutdown) {
        if ((uint32_t)(now - s->started) >= POWER_HOLD_MS)
            result = INPUT_POWER_SHUTDOWN;
        else if (s->second)
            result = INPUT_POWER_DISPLAYIMG;
        else {
            s->pending = true;
            s->released = now;
        }
    }
    s->second = false;
    s->blocked = false;
    return result;
}

input_action_t power_gesture_tick(power_gesture_t *s, uint32_t now)
{
    if (s->pending && !s->down && (uint32_t)(now - s->released) >= POWER_DOUBLE_MS) {
        s->pending = false;
        return INPUT_POWER_MENU;
    }
    if (!s->down || s->blocked || s->fired ||
        (uint32_t)(now - s->started) < POWER_HOLD_MS) return INPUT_NONE;
    s->fired = true;
    return INPUT_POWER_SHUTDOWN;
}

void touch_gesture_cancel(touch_gesture_t *s)
{
    s->blocked = true;
    s->remainder = 0;
}

input_action_t touch_gesture_frame(touch_gesture_t *s, bool down, int x,
                                   unsigned flags, uint32_t now, bool shutdown,
                                   int *steps)
{
    *steps = 0;
    if ((flags & (TOUCH_MULTI | TOUCH_CANCEL)) ||
        (down && !(flags & TOUCH_VALID))) touch_gesture_cancel(s);
    if (s->down && s->shutdown != shutdown) touch_gesture_cancel(s);
    if (!down) {
        input_action_t action = INPUT_NONE;
        if (s->down && !s->blocked) {
            uint32_t elapsed = now - s->started;
            if (s->shutdown) {
                if (s->last - s->start >= TOUCH_CONFIRM_DISTANCE && s->reverse <= TOUCH_CONFIRM_REVERSE)
                    action = INPUT_TOUCH_DOWN_CONFIRM;
            } else if (!s->moved && !s->held && elapsed >= TOUCH_TAP_MIN_MS && elapsed <= TOUCH_TAP_MAX_MS) {
                action = INPUT_TOUCH_TAP;
            }
        }
        s->down = false;
        s->blocked = false;
        return action;
    }
    if (!s->down) {
        s->down = true;
        s->shutdown = shutdown;
        s->started = now;
        s->start = s->last = x;
        s->remainder = s->max_offset = s->reverse = 0;
        s->moved = s->held = false;
        return INPUT_NONE;
    }
    if (s->blocked) return INPUT_NONE;
    int delta = x - s->last;
    s->last = x;
    if (delta < 0) s->reverse -= delta;
    int offset = abs(x - s->start);
    if (offset > s->max_offset) s->max_offset = offset;
    if (s->max_offset >= TOUCH_MOVE_SLOP) s->moved = true;
    if (!shutdown) {
        s->remainder += delta;
        *steps = s->remainder / TOUCH_NAV_STEP;
        s->remainder -= *steps * TOUCH_NAV_STEP;
    }
    return INPUT_NONE;
}

input_action_t touch_gesture_tick(touch_gesture_t *s, uint32_t now)
{
    if (!s->down || s->blocked || s->moved || s->held || s->shutdown ||
        (uint32_t)(now - s->started) < TOUCH_HOLD_MS) return INPUT_NONE;
    s->held = true;
    return INPUT_TOUCH_HOLD;
}
