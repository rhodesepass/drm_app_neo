#pragma once
#include "drm_warpper.h"

typedef struct {
    drm_warpper_t *drm_warpper;
} layer_animation_t;

typedef struct {
    int layer_id;
    int16_t *xarr;
    int16_t *yarr;
    int i;
    int step_count;
    drm_warpper_t *drm_warpper;
} layer_animation_move_data_t;

typedef struct {
    int layer_id;
    int16_t *alphaarr;
    int i;
    int step_count;
    drm_warpper_t *drm_warpper;
} layer_animation_fade_data_t;

int layer_animation_init(layer_animation_t *layer_animation,drm_warpper_t *drm_warpper);

int layer_animation_fade_in(layer_animation_t *layer_animation,int layer_id,int duration,int delay);
int layer_animation_fade_out(layer_animation_t *layer_animation,int layer_id,int duration,int delay);
int layer_animation_linear_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int duration,int delay);
int layer_animation_cubic_bezier_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int ctlx1,int ctly1,int ctx2,int cty2,int duration,int delay);
int layer_animation_ease_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int duration,int delay);
int layer_animation_ease_in_out_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int duration,int delay);
int layer_animation_ease_out_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int duration,int delay);
int layer_animation_ease_in_move(layer_animation_t *layer_animation,int layer_id,int xs,int ys,int xe,int ye,int duration,int delay);
