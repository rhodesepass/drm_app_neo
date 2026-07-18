#ifdef _WIN32
// Windows(mingw) 无 libdrm。PC/SDL 后端不接 DRM，给 drm 类型/宏占位即可——
// drm_warpper_t 里那几个 drm 字段在 SDL 后端根本不会被触碰。Linux PC 仍走真头。
typedef struct { int _unused; } drmVBlank;
typedef struct drmModeConnector drmModeConnector;   // 前向声明，仅作指针
typedef struct drmModeRes drmModeRes;
typedef struct drmModePlaneRes drmModePlaneRes;
#ifndef fourcc_mod_code
#define fourcc_mod_code(vendor, val) 0ULL
#endif
#else
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <semaphore.h>
#endif
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "utils/spsc_queue.h"

#pragma once


#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)

typedef enum {
    DRM_WARPPER_LAYER_MODE_RGB565,
    DRM_WARPPER_LAYER_MODE_ARGB8888,
    DRM_WARPPER_LAYER_MODE_MB32_NV12, //allwinner specific format
    DRM_WARPPER_LAYER_MODE_C8,        // 256 色调色板(DEBE palette 模式, 内核 patch 0029)
} drm_warpper_layer_mode_t;

typedef enum {
    // atomic：整帧翻页，fb_id 换到该层 plane 的 FB_ID 属性
    DRM_WARPPER_ITEM_FLIP_FB,
    // atomic：CRTC_X/CRTC_Y（有符号，可为负/离屏，过渡动画依赖）
    DRM_WARPPER_ITEM_SET_COORD,
    // atomic：plane alpha。255 = 不透明(ARGB 层回到像素 alpha)
    DRM_WARPPER_ITEM_SET_ALPHA,
} drm_warpper_item_type_t;

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
    drm_warpper_item_type_t type;
    uint32_t fb_id;         // FLIP_FB
    int16_t x, y;           // SET_COORD
    uint8_t alpha;          // SET_ALPHA
    void* userdata;
    bool on_heap;
} drm_warpper_queue_item_t;

typedef struct{
    bool used;
    spsc_bq_t display_queue;
    spsc_bq_t free_queue;
    drm_warpper_layer_mode_t mode;
    int width;
    int height;
    // 阻塞 commit 返回即旧帧离屏，在屏只押 curr 一格(试过 NONBLOCK+
    // PAGE_FLIP_EVENT 在飞翻页，要多押一个 pending，为 32MB 机型撤回)
    drm_warpper_queue_item_t* curr_item;
    // 惰性挂载(video 层专用)：几何先存这，显示线程翻第一帧时连同
    // CRTC_ID/SRC_*/CRTC_* 一起 commit——plane 用真实帧启用，无需黑 buffer
    bool needs_full_mount;
    int16_t geo_x, geo_y;
    int geo_src_w, geo_src_h, geo_dst_w, geo_dst_h;
} layer_t;

// 按名字发现的 plane 属性 id（atomic commit 用）；0 = 无此属性
typedef struct {
    uint32_t fb_id, crtc_id;
    uint32_t src_x, src_y, src_w, src_h;
    uint32_t crtc_x, crtc_y, crtc_w, crtc_h;
    uint32_t alpha;
    uint32_t zpos;
} plane_prop_ids_t;

typedef struct {
  int fd;
  drmModeConnector *conn;
  drmModeRes *res;
  drmModePlaneRes *plane_res;
  uint32_t crtc_id;
  uint32_t conn_id;
  layer_t layer[4]; // 4 layers
  uint32_t plane_ids[4];
  plane_prop_ids_t plane_props[4];
  // 序列化 mount(同步 commit) 与显示线程的每 vsync commit
  pthread_mutex_t commit_mutex;
  drmVBlank blank;
  pthread_t display_thread;
  atomic_int thread_running;
} drm_warpper_t;



int drm_warpper_init(drm_warpper_t *drm_warpper);
int drm_warpper_destroy(drm_warpper_t *drm_warpper);

int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode);
int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id);
int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf);
// 通用:src=(0,0,src_w,src_h) 从 buf 左上角裁,dst=(x,y,dst_w,dst_h) 屏幕显示区。
// src != dst 走 DEFE 硬件缩放;src_w<buf->width 裁掉对齐 padding。裁切与缩放可组合。仅 MB32_NV12(video)层支持缩放。
int drm_warpper_mount_layer_rect(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf,int src_w,int src_h,int dst_w,int dst_h);


int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf);
int drm_warpper_allocate_buffer_sized(drm_warpper_t *drm_warpper,int layer_id,int width,int height,buffer_object_t *buf);
int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf);

// dmabuf(如 cedrus capture buffer) 导入为 NV12 + ALLWINNER_TILED 的 DRM FB。
// pitch/uv_offset 按 V4L2 G_FMT 回读值传入。gem_handle 回传 prime 句柄，
// rm_fb 时必须一并传回 close，否则 CMA 被 pin 住不归还。
int drm_warpper_import_dmabuf_fb(drm_warpper_t *drm_warpper,int dmabuf_fd,int width,int height,int pitch,int uv_offset,uint32_t *fb_id,uint32_t *gem_handle);
int drm_warpper_rm_fb(drm_warpper_t *drm_warpper,uint32_t fb_id,uint32_t gem_handle);
// 惰性挂载：只记录几何，plane 由显示线程在下一个 FLIP_FB 时携带真实帧启用。
// 调用时机须保证该层没有在飞的 FLIP(解码线程未跑)
int drm_warpper_set_layer_geometry(drm_warpper_t *drm_warpper,int layer_id,int x,int y,int src_w,int src_h,int dst_w,int dst_h);
// 同步 disable plane(CRTC_ID/FB_ID=0)。最底层关掉后露出 DEBE 背景色(黑)；
// 之后的 RmFB 不会碰到在屏 fb。需要 suniv quirks 内核(0014)否则可能被 atomic_check 拒
int drm_warpper_disable_layer_sync(drm_warpper_t *drm_warpper,int layer_id);

int drm_warpper_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item);
int drm_warpper_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item);
int drm_warpper_try_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item);

int drm_warpper_set_layer_coord(drm_warpper_t *drm_warpper,int layer_id,int x,int y);
int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha);

// C8 调色板上传:256 项小端 ARGB8888。设备侧写 sysfs(立即锁存,不等 vsync,
// 在屏内容会当场换色——调用方自行保证时机);SDL 侧存软副本供合成线程展开。
int drm_warpper_set_palette(drm_warpper_t *drm_warpper,const uint32_t pal[256]);
