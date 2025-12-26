#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include "spsc_queue.h"
#include "srgn_drm.h"
#include "stdbool.h"

#pragma once


#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)

typedef enum {
    DRM_WARPPER_LAYER_MODE_RGB565,
    DRM_WARPPER_LAYER_MODE_ARGB8888,
    DRM_WARPPER_LAYER_MODE_MB32_NV12, //allwinner specific format
} drm_warpper_layer_mode_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
} buffer_object_t;

typedef struct {
    struct drm_srgn_mount_fb mount;
    void* userdata;
} drm_warpper_queue_item_t;

typedef struct{
    bool used;
    spsc_bq_t display_queue;
    spsc_bq_t free_queue;
    drm_warpper_layer_mode_t mode;
    int width;
    int height;
    drm_warpper_queue_item_t* curr_item;
} layer_t;


typedef struct {
  int fd;
  drmModeConnector *conn;
  drmModeRes *res;
  drmModePlaneRes *plane_res;
  uint32_t crtc_id;
  uint32_t conn_id;
  layer_t layer[4]; // 4 layers
  drmVBlank blank;
  pthread_t display_thread;
  int thread_running;
} drm_warpper_t;



int drm_warpper_init(drm_warpper_t *drm_warpper);
int drm_warpper_destroy(drm_warpper_t *drm_warpper);

int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode);
int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id);
int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf);


int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf);
int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf);

int drm_warpper_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item);
int drm_warpper_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item);
int drm_warpper_try_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item);
