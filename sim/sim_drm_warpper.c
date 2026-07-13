//
// drm_warpper 的 SDL 仿真侧 mock —— 只实现 overlay 链路引用的 5 个函数
// (allocate/free/mount buffer + set coord/alpha)。buffer 是普通 malloc 内存，
// 层状态存在文件内，由 overlay_sim_main 的合成器每帧读取后用 SDL 混叠，
// 等价设备侧 DEBE 的图层叠加。
//
// 跨线程约定：overlay worker / prts_timer 线程写坐标与 alpha，合成器线程读。
// 都是对齐整型的裸读写，撕裂语义与真机"绘制期间撞上扫描"相同，仿真可接受。
//
#include "driver/drm_warpper.h"
#include "utils/log.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int inited;
    int width, height;
    drm_warpper_layer_mode_t mode;
    uint8_t* vaddr; // 当前挂载的 buffer（mount_layer 记录）
    int16_t x, y;
    uint8_t alpha;
} sim_layer_t;

static sim_layer_t s_layers[4];

int drm_warpper_init(drm_warpper_t *drm_warpper){
    memset(drm_warpper, 0, sizeof(*drm_warpper));
    memset(s_layers, 0, sizeof(s_layers));
    log_info("==> [sim] drm_warpper mock initialized");
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    (void)drm_warpper;
    return 0;
}

int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4) return -1;
    s_layers[layer_id].inited = 1;
    s_layers[layer_id].width = width;
    s_layers[layer_id].height = height;
    s_layers[layer_id].mode = mode;
    s_layers[layer_id].alpha = 255;
    return 0;
}

int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4) return -1;
    s_layers[layer_id].inited = 0;
    return 0;
}

int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4 || !s_layers[layer_id].inited) return -1;

    int w = s_layers[layer_id].width;
    int h = s_layers[layer_id].height;
    int bpp = (s_layers[layer_id].mode == DRM_WARPPER_LAYER_MODE_RGB565) ? 2 : 4;

    memset(buf, 0, sizeof(*buf));
    buf->width = w;
    buf->height = h;
    buf->pitch = w * bpp;
    buf->size = (uint32_t)(w * h * bpp);
    buf->vaddr = calloc(1, buf->size);
    return buf->vaddr ? 0 : -1;
}

int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    (void)drm_warpper;
    (void)layer_id;
    free(buf->vaddr);
    buf->vaddr = NULL;
    return 0;
}

int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4) return -1;
    s_layers[layer_id].vaddr = buf->vaddr;
    s_layers[layer_id].x = (int16_t)x;
    s_layers[layer_id].y = (int16_t)y;
    return 0;
}

int drm_warpper_set_layer_coord(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4) return -1;
    s_layers[layer_id].x = (int16_t)x;
    s_layers[layer_id].y = (int16_t)y;
    return 0;
}

int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha){
    (void)drm_warpper;
    if(layer_id < 0 || layer_id >= 4) return -1;
    s_layers[layer_id].alpha = (uint8_t)alpha;
    return 0;
}

// 合成器读取层状态（overlay_sim_main 用）
void sim_drm_layer_state(int layer_id, uint8_t** vaddr, int* x, int* y, int* alpha){
    *vaddr = s_layers[layer_id].vaddr;
    *x = s_layers[layer_id].x;
    *y = s_layers[layer_id].y;
    *alpha = s_layers[layer_id].alpha;
}
