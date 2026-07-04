#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <unistd.h>

#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "config.h"
#include "utils/spsc_queue.h"

static void drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;
    if (drmWaitVBlank(drm_warpper->fd, (drmVBlankPtr) &drm_warpper->blank)) {
      log_error("drmWaitVBlank failed");
    }
}

// alpha 属性 0..0xFFFF；255*0x101 = 0xFFFF 正好回到不透明(像素 alpha)
static inline uint64_t alpha_to_prop(uint8_t alpha){
    return (uint64_t)alpha * 0x101;
}

static int drm_warpper_discover_plane_props(drm_warpper_t *drm_warpper, int layer_id){
    plane_prop_ids_t *p = &drm_warpper->plane_props[layer_id];
    drmModeObjectProperties *props;
    int i;

    memset(p, 0, sizeof(*p));

    props = drmModeObjectGetProperties(drm_warpper->fd,
                                       drm_warpper->plane_ids[layer_id],
                                       DRM_MODE_OBJECT_PLANE);
    if (!props) {
        log_error("get plane %d properties failed", layer_id);
        return -1;
    }

    for (i = 0; i < (int)props->count_props; i++) {
        drmModePropertyRes *prop = drmModeGetProperty(drm_warpper->fd, props->props[i]);
        if (!prop)
            continue;
        if      (!strcmp(prop->name, "FB_ID"))   p->fb_id   = prop->prop_id;
        else if (!strcmp(prop->name, "CRTC_ID")) p->crtc_id = prop->prop_id;
        else if (!strcmp(prop->name, "SRC_X"))   p->src_x   = prop->prop_id;
        else if (!strcmp(prop->name, "SRC_Y"))   p->src_y   = prop->prop_id;
        else if (!strcmp(prop->name, "SRC_W"))   p->src_w   = prop->prop_id;
        else if (!strcmp(prop->name, "SRC_H"))   p->src_h   = prop->prop_id;
        else if (!strcmp(prop->name, "CRTC_X"))  p->crtc_x  = prop->prop_id;
        else if (!strcmp(prop->name, "CRTC_Y"))  p->crtc_y  = prop->prop_id;
        else if (!strcmp(prop->name, "CRTC_W"))  p->crtc_w  = prop->prop_id;
        else if (!strcmp(prop->name, "CRTC_H"))  p->crtc_h  = prop->prop_id;
        else if (!strcmp(prop->name, "alpha"))   p->alpha   = prop->prop_id;
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    if (!p->fb_id || !p->crtc_id || !p->crtc_x || !p->crtc_y) {
        log_error("plane %d missing required properties", layer_id);
        return -1;
    }
    return 0;
}

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    int ret;

    log_info("==> DRM_Warpper Display Thread Started!");

    // 节奏：有活时直接阻塞 commit（commit 本身按 vblank 节拍返回，
    // 背靠背即每 vblank 一次）；空转时用 drmWaitVBlank 兜底等待。
    while(atomic_load(&drm_warpper->thread_running)){
        drmModeAtomicReq *req = NULL;

        for(int i = 0; i < 4; i++){
            layer_t* layer = &drm_warpper->layer[i];
            plane_prop_ids_t *p = &drm_warpper->plane_props[i];
            if(!layer->used)
                continue;
            drm_warpper_queue_item_t* item;
            while(spsc_bq_try_pop(&layer->display_queue, (void**)&item) == 0){
                bool is_frame = false;

                switch(item->type){
                case DRM_WARPPER_ITEM_FLIP_FB:
                    if(!req) req = drmModeAtomicAlloc();
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->fb_id, item->fb_id);
                    is_frame = true;
                    break;
                case DRM_WARPPER_ITEM_SET_COORD:
                    if(!req) req = drmModeAtomicAlloc();
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->crtc_x, (uint64_t)(int64_t)item->x);
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->crtc_y, (uint64_t)(int64_t)item->y);
                    break;
                case DRM_WARPPER_ITEM_SET_ALPHA:
                    if(!p->alpha){
                        log_error("layer %d has no alpha property", i);
                        break;
                    }
                    if(!req) req = drmModeAtomicAlloc();
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->alpha, alpha_to_prop(item->alpha));
                    break;
                }

                if(is_frame){
                    // 帧类 item 常驻(非堆)：换下的旧帧回 free_queue 供解码侧回收
                    if(layer->curr_item){
                        // 如果程序终止 对端consumer可能已经退出 导致这里卡死。
                        spsc_bq_push(&layer->free_queue, layer->curr_item);
                    }
                    layer->curr_item = item;
                }
                else{
                    if(item->on_heap){
                        free(item);
                    }
                }
            }
        }

        if(req){
            // 阻塞式：返回即 flip 完成（下一个 vblank），被换下的 FB 已
            // 离屏——此后旧帧 item 才会从 free_queue 被解码侧回收复用。
            pthread_mutex_lock(&drm_warpper->commit_mutex);
            ret = drmModeAtomicCommit(drm_warpper->fd, req, 0, NULL);
            pthread_mutex_unlock(&drm_warpper->commit_mutex);
            if(ret < 0){
                log_error("drmModeAtomicCommit failed %s(%d)", strerror(errno), errno);
            }
            drmModeAtomicFree(req);
        }
        else{
            drm_warpper_wait_for_vsync(drm_warpper);
        }
    }

    log_info("==> DRM_Warpper Display Thread Ended!");
    return NULL;
}

int drm_warpper_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_push(&layer->display_queue, item);
}

int drm_warpper_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_pop(&layer->free_queue, (void**)out_item);
}

int drm_warpper_try_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_try_pop(&layer->free_queue, (void**)out_item);
}

int drm_warpper_set_layer_coord(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    drm_warpper_queue_item_t *item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_SET_COORD;
    item->x = (int16_t)x;
    item->y = (int16_t)y;
    item->on_heap = true;
#ifndef APP_RELEASE
    log_trace("drm coord y:%d,x:%d",y,x);
#endif // APP_RELEASE
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha){
    drm_warpper_queue_item_t *item;

    // sun4i atomic_check：最底已启用 plane 的属性 alpha 必须 opaque，且全局
    // 只允许 1 个 alpha 平面(含 ARGB 像素 alpha 的 overlay)。只有 overlay 能动。
    if(layer_id != DRM_WARPPER_LAYER_OVERLAY && alpha != 255){
        log_warn("alpha on layer %d rejected (only overlay may fade)", layer_id);
        return -1;
    }

    item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_SET_ALPHA;
    item->alpha = (uint8_t)alpha;
    item->on_heap = true;
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_init(drm_warpper_t *drm_warpper){
    int ret;

    memset(drm_warpper, 0, sizeof(drm_warpper_t));

    drm_warpper->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_warpper->fd < 0) {
        log_error("open /dev/dri/card0 failed");
        return -1;
    }

    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if(ret) {
        log_error("No atomic modesetting support: %s", strerror(errno));
        close(drm_warpper->fd);
        return -1;
    }

    drm_warpper->res = drmModeGetResources(drm_warpper->fd);
    if (!drm_warpper->res || drm_warpper->res->count_crtcs == 0 || drm_warpper->res->count_connectors == 0) {
        log_error("drmModeGetResources failed or no CRTCs/connectors");
        close(drm_warpper->fd);
        return -1;
    }
    drm_warpper->crtc_id = drm_warpper->res->crtcs[0];
    drm_warpper->conn_id = drm_warpper->res->connectors[0];

    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
      log_error("failed to set client cap\n");
      drmModeFreeResources(drm_warpper->res);
      close(drm_warpper->fd);
      return -1;
    }
    drm_warpper->plane_res = drmModeGetPlaneResources(drm_warpper->fd);
    if (!drm_warpper->plane_res) {
        log_error("drmModeGetPlaneResources failed");
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }
    log_info("Available Plane Count: %d", drm_warpper->plane_res->count_planes);

    for(uint32_t i = 0; i < 4 && i < drm_warpper->plane_res->count_planes; i++){
        drm_warpper->plane_ids[i] = drm_warpper->plane_res->planes[i];
        if(drm_warpper_discover_plane_props(drm_warpper, i) < 0){
            drmModeFreePlaneResources(drm_warpper->plane_res);
            drmModeFreeResources(drm_warpper->res);
            close(drm_warpper->fd);
            return -1;
        }
    }

    drm_warpper->conn = drmModeGetConnector(drm_warpper->fd, drm_warpper->conn_id);
    if (!drm_warpper->conn) {
        log_error("drmModeGetConnector failed");
        drmModeFreePlaneResources(drm_warpper->plane_res);
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }

    log_info("Connector Name: %s, %dx%d, Refresh Rate: %d",
        drm_warpper->conn->modes[0].name, drm_warpper->conn->modes[0].vdisplay, drm_warpper->conn->modes[0].hdisplay,
        drm_warpper->conn->modes[0].vrefresh);

    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;

    pthread_mutex_init(&drm_warpper->commit_mutex, NULL);

    atomic_store(&drm_warpper->thread_running, 1);
    if (pthread_create(&drm_warpper->display_thread, NULL, drm_warpper_display_thread, drm_warpper) != 0) {
        log_error("Failed to create display thread");
        drmModeFreeConnector(drm_warpper->conn);
        drmModeFreePlaneResources(drm_warpper->plane_res);
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    // 先停线程再释放资源（原实现先 close fd，线程还在用）
    atomic_store(&drm_warpper->thread_running, 0);
    log_info("wait for display thread to finish");
    pthread_join(drm_warpper->display_thread, NULL);
    log_info("display thread finished");

    for(int i = 0; i < 4; i++){
        drm_warpper_destroy_layer(drm_warpper, i);
    }

    drmModeFreeConnector(drm_warpper->conn);
    drmModeFreePlaneResources(drm_warpper->plane_res);
    drmModeFreeResources(drm_warpper->res);
    pthread_mutex_destroy(&drm_warpper->commit_mutex);
    close(drm_warpper->fd);

    return 0;
}

static int drm_warpper_create_buffer_object(int fd,buffer_object_t* bo,int width,int height,drm_warpper_layer_mode_t mode){
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    uint32_t handles[4], pitches[4], offsets[4];
    uint64_t modifiers[4];
    int ret;

    memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        creq.width = width;
        creq.height = height * 3 / 2;
        creq.bpp = 8;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        creq.width = width;
        creq.height = height;
        creq.bpp = 16;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        creq.width = width;
        creq.height = height;
        creq.bpp = 32;
    }
    else{
        log_error("invalid layer mode");
        return -1;
    }


    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
      log_error("cannot create dumb buffer (%d): %m", errno);
      return -errno;
    }

    memset(&offsets, 0, sizeof(offsets));
    memset(&handles, 0, sizeof(handles));
    memset(&pitches, 0, sizeof(pitches));
    memset(&modifiers, 0, sizeof(modifiers));

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = DRM_FORMAT_MOD_ALLWINNER_TILED;

        offsets[1] = creq.pitch * height;
        handles[1] = creq.handle;
        pitches[1] = creq.pitch;
        modifiers[1] = DRM_FORMAT_MOD_ALLWINNER_TILED;
    }
    else{
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = 0;
    }

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        ret = drmModeAddFB2WithModifiers(fd, width, height, DRM_FORMAT_NV12, handles,
                                     pitches, offsets, modifiers, &bo->fb_id,
                                     DRM_MODE_FB_MODIFIERS);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_RGB565, handles, pitches, offsets,&bo->fb_id, 0);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_ARGB8888, handles, pitches, offsets,&bo->fb_id, 0);
    }

    if (ret) {
      log_error("drmModeAddFB2 return err %d", ret);
      return -1;
    }

    /* prepare buffer for memory mapping */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
      log_error("1st cannot map dumb buffer (%d): %m\n", errno);
      return -1;
    }
    /* perform actual memory mapping */
    bo->vaddr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);

    if (bo->vaddr == MAP_FAILED) {
        log_error("2nd cannot mmap dumb buffer (%d): %m\n", errno);
      return -1;
    }

    bo->handle = creq.handle;
    bo->pitch = creq.pitch;
    bo->size = creq.size;

    return 0;
}


int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode){

    layer_t* layer = &drm_warpper->layer[layer_id];
    int ret;

    ret = spsc_bq_init(&layer->display_queue, 16);
    if(ret < 0){
        log_error("failed to initialize display queue");
        return -1;
    }
    ret = spsc_bq_init(&layer->free_queue, 2);
    if(ret < 0){
        log_error("failed to initialize free queue");
        return -1;
    }

    layer->mode = mode;
    layer->used = true;
    layer->width = width;
    layer->height = height;

    layer->curr_item = NULL;

    return 0;
}

int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    layer_t* layer = &drm_warpper->layer[layer_id];
    if(!layer->used){
        return 0;
    }
    spsc_bq_destroy(&layer->display_queue);
    spsc_bq_destroy(&layer->free_queue);
    layer->used = false;
    return 0;
}

int drm_warpper_allocate_buffer_sized(drm_warpper_t *drm_warpper,int layer_id,int width,int height,buffer_object_t *buf){
    int ret;
    layer_t* layer = &drm_warpper->layer[layer_id];
    buf->width = width;
    buf->height = height;
    ret = drm_warpper_create_buffer_object(drm_warpper->fd, buf, width, height, layer->mode);
    if(ret < 0){
        log_error("failed to allocate buffer");
        return -1;
    }
    return 0;
}

int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return drm_warpper_allocate_buffer_sized(drm_warpper, layer_id, layer->width, layer->height, buf);
}

int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    struct drm_mode_destroy_dumb destroy;

    memset(&destroy, 0, sizeof(struct drm_mode_destroy_dumb));

    drmModeRmFB(drm_warpper->fd, buf->fb_id);
    munmap(buf->vaddr, buf->size);

    destroy.handle = buf->handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    return 0;
}

int drm_warpper_import_dmabuf_fb(drm_warpper_t *drm_warpper,int dmabuf_fd,int width,int height,int pitch,int uv_offset,uint32_t *fb_id){
    uint32_t handle = 0;
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    int ret;

    ret = drmPrimeFDToHandle(drm_warpper->fd, dmabuf_fd, &handle);
    if(ret < 0){
        log_error("drmPrimeFDToHandle failed %s(%d)", strerror(errno), errno);
        return -1;
    }

    handles[0] = handles[1] = handle;
    pitches[0] = pitches[1] = pitch;
    offsets[0] = 0;
    offsets[1] = uv_offset;
    modifiers[0] = modifiers[1] = DRM_FORMAT_MOD_ALLWINNER_TILED;

    ret = drmModeAddFB2WithModifiers(drm_warpper->fd, width, height,
                                     DRM_FORMAT_NV12, handles, pitches,
                                     offsets, modifiers, fb_id,
                                     DRM_MODE_FB_MODIFIERS);
    if(ret < 0){
        log_error("import dmabuf AddFB2 failed %s(%d)", strerror(errno), errno);
        return -1;
    }
    return 0;
}

int drm_warpper_rm_fb(drm_warpper_t *drm_warpper,uint32_t fb_id){
    return drmModeRmFB(drm_warpper->fd, fb_id);
}


// 通用挂载:src 矩形(0,0,src_w,src_h)从 buf 左上角裁,dst 矩形(x,y,dst_w,dst_h)是屏幕显示区。
//   src==dst        -> 1:1
//   src<dst / src>dst -> DEFE frontend 硬件缩放(仅 MB32 NV12 video 层;DEBE 无 scaler)
//   src_w<buf->width -> 裁掉右侧对齐 padding
// 三者可组合:如 src=(360,720) dst=(720,H) 即"先裁左 360 再放大到 720"。
// 同步 atomic commit(首挂即启用 plane;CRTC 开机已 active,无需 ALLOW_MODESET)
int drm_warpper_mount_layer_rect(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf,
                                 int src_w,int src_h,int dst_w,int dst_h){
    plane_prop_ids_t *p = &drm_warpper->plane_props[layer_id];
    uint32_t plane_id = drm_warpper->plane_ids[layer_id];
    drmModeAtomicReq *req;
    int ret;

    req = drmModeAtomicAlloc();
    if(!req){
        log_error("drmModeAtomicAlloc failed");
        return -1;
    }

    drmModeAtomicAddProperty(req, plane_id, p->crtc_id, drm_warpper->crtc_id);
    drmModeAtomicAddProperty(req, plane_id, p->fb_id, buf->fb_id);
    drmModeAtomicAddProperty(req, plane_id, p->src_x, 0);
    drmModeAtomicAddProperty(req, plane_id, p->src_y, 0);
    drmModeAtomicAddProperty(req, plane_id, p->src_w, (uint64_t)src_w << 16);
    drmModeAtomicAddProperty(req, plane_id, p->src_h, (uint64_t)src_h << 16);
    drmModeAtomicAddProperty(req, plane_id, p->crtc_x, (uint64_t)(int64_t)x);
    drmModeAtomicAddProperty(req, plane_id, p->crtc_y, (uint64_t)(int64_t)y);
    drmModeAtomicAddProperty(req, plane_id, p->crtc_w, dst_w);
    drmModeAtomicAddProperty(req, plane_id, p->crtc_h, dst_h);

    pthread_mutex_lock(&drm_warpper->commit_mutex);
    ret = drmModeAtomicCommit(drm_warpper->fd, req, 0, NULL);
    pthread_mutex_unlock(&drm_warpper->commit_mutex);
    drmModeAtomicFree(req);

    if (ret < 0)
        log_error("mount atomic commit err %s(%d)", strerror(errno), errno);
    return ret;
}

// src 恒为整幅 buf,dst != buf 时走 DEFE 缩放
int drm_warpper_mount_layer_scaled(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf,int dst_w,int dst_h){
    return drm_warpper_mount_layer_rect(drm_warpper, layer_id, x, y, buf, buf->width, buf->height, dst_w, dst_h);
}

int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    return drm_warpper_mount_layer_rect(drm_warpper, layer_id, x, y, buf, buf->width, buf->height, buf->width, buf->height);
}

// src=dst=crop:只取 buf 左上角 crop_w×crop_h,1:1 贴屏,裁掉右侧/下方对齐 padding,不缩放
int drm_warpper_mount_layer_cropped(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf,int crop_w,int crop_h){
    return drm_warpper_mount_layer_rect(drm_warpper, layer_id, x, y, buf, crop_w, crop_h, crop_w, crop_h);
}
