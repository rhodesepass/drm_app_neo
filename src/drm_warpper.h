#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdint.h>
#include "mcmpq.h"
#include <semaphore.h>
#include <pthread.h>

#pragma once


#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)

typedef enum {
    DRM_WARPPER_BUFFER_STATE_FREE, // ready to be draw
    DRM_WARPPER_BUFFER_STATE_DRAWING, // drawing
    DRM_WARPPER_BUFFER_STATE_READY, // ready to be shown
    DRM_WARPPER_BUFFER_STATE_DISPLAYING, // displaying
} drm_warpper_buffer_state_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
    drm_warpper_buffer_state_t state;
    sem_t *related_free_sem;
} buffer_object_t;

typedef struct{
    buffer_object_t buf[2];
    bool used;
    queue_t ready_queue;
    sem_t free_sem;
} plane_t;

typedef struct {
    uint8_t *ch0_addr;
    uint8_t *ch1_addr;
    uint8_t *ch2_addr;
    int layer_id;
    sem_t direct_commit_sem;
} drm_warpper_direct_commit_t;

typedef struct {
  int fd;
  drmModeConnector *conn;
  drmModeRes *res;
  drmModePlaneRes *plane_res;
  uint32_t crtc_id;
  uint32_t conn_id;
  plane_t plane[4]; // 4 layers, 2 buffers per layer
  drmVBlank blank;
  pthread_t display_thread;
  int thread_running;
} drm_warpper_t;

typedef enum {
    DRM_WARPPER_LAYER_MODE_RGB565,
    DRM_WARPPER_LAYER_MODE_ARGB8888,
    DRM_WARPPER_LAYER_MODE_MB32_NV12, //allwinner specific format
} drm_warpper_layer_mode_t;

int drm_warpper_init(drm_warpper_t *drm_warpper);
int drm_warpper_destroy(drm_warpper_t *drm_warpper);
int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode);
int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id);
int drm_warpper_get_layer_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr);
int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y);

int drm_warpper_arquire_draw_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr);
int drm_warpper_return_draw_buffer(drm_warpper_t *drm_warpper,int layer_id, uint8_t* vaddr);

int drm_warpper_direct_commit(drm_warpper_t *drm_warpper,int layer_id,uint8_t *ch0_addr,uint8_t *ch1_addr,uint8_t *ch2_addr);