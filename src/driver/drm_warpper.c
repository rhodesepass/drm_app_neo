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
#include "driver/srgn_drm.h"
#include "config.h"
#include "utils/spsc_queue.h"

static inline int DRM_IOCTL(int fd, unsigned long cmd, void *arg) {
  int ret = drmIoctl(fd, cmd, arg);
  return ret < 0 ? -errno : ret;
}

static void drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;
    // log_info("wait for vsync");
    if (drmWaitVBlank(drm_warpper->fd, (drmVBlankPtr) &drm_warpper->blank)) {
      log_error("drmWaitVBlank failed");
    }
    // log_info("vsync done");
}

// 内核那边会缓存用户地址到物理地址，以便快速挂。
// 每次启动前都需要reset cache，避免上次启动的残留。
void drm_warpper_reset_cache_ioctl(drm_warpper_t *drm_warpper){
    int ret;
    ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_RESET_FB_CACHE, NULL);
    if(ret < 0){
        log_error("drm_warpper_reset_cache_ioctl failed %s(%d)", strerror(errno), errno);
    }
}

// static void drm_warpper_switch_buffer_ioctl(drm_warpper_t *drm_warpper,int layer_id,int type,uint8_t *ch0_addr,uint8_t *ch1_addr,uint8_t *ch2_addr){
//     int ret;
//     struct drm_srgn_mount_fb srgn_mount_fb;

//     srgn_mount_fb.layer_id = layer_id;
//     srgn_mount_fb.type = type;
//     srgn_mount_fb.ch0_addr = (uint32_t)ch0_addr;
//     srgn_mount_fb.ch1_addr = (uint32_t)ch1_addr;
//     srgn_mount_fb.ch2_addr = (uint32_t)ch2_addr;
    
//     ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_MOUNT_FB, &srgn_mount_fb);
//     if(ret < 0){
//         log_error("drm_warpper_switch_buffer_ioctl failed %s(%d)", strerror(errno), errno);
//     }
// }

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    struct drm_srgn_atomic_commit_data commits[12] = {0};
    struct drm_srgn_atomic_commit commit_req = {
        .size = 0,
        .data = (uint32_t)(uintptr_t)commits,
    };
    int ret;

    log_info("==> DRM_Warpper Display Thread Started!");

    while(drm_warpper->thread_running){
        drm_warpper_wait_for_vsync(drm_warpper);
        // log_info("vsync");
        commit_req.size = 0;
        for(int i = 0; i < 4; i++){
            layer_t* layer = &drm_warpper->layer[i];
            if(layer->used){
                drm_warpper_queue_item_t* item;
                while(spsc_bq_try_pop(&layer->display_queue, (void**)&item) == 0){
                    // somthing is wait to be displayed.
                    // so, switch buffer using ioctl,and put current item to free queue.
                    // log_info("switch buffer on layer %d type %d", i, item->mount.type);
                    commits[commit_req.size].layer_id = i;
                    commits[commit_req.size].type = item->mount.type;
                    commits[commit_req.size].arg0 = item->mount.arg0;
                    commits[commit_req.size].arg1 = item->mount.arg1;
                    commits[commit_req.size].arg2 = item->mount.arg2;
                    commit_req.size++;
                    if(item->mount.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL 
                        || item->mount.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV){
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
        }
        if(commit_req.size > 0){
            ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit_req);
            if(ret < 0){
                log_error("DRM_IOCTL_SRGN_ATOMIC_COMMIT failed %s(%d)", strerror(errno), errno);
            }
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
    item->mount.layer_id = layer_id;
    item->mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_COORD;
    item->mount.arg0 = (int16_t)y << 16 | (int16_t)x;
    item->mount.arg1 = 0;
    item->mount.arg2 = 0;
    item->userdata = NULL;
    item->on_heap = true;
#ifndef APP_RELEASE
    log_trace("drm coord y:%d,x:%d,reg:%x",y,x,item->mount.arg0);
#endif // APP_RELEASE
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha){
    drm_warpper_queue_item_t *item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    item->mount.layer_id = layer_id;
    item->mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA;
    item->mount.arg0 = alpha;
    item->mount.arg1 = 0;
    item->mount.arg2 = 0;
    item->userdata = NULL;
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
        return -1;
    }
    
    drm_warpper->res = drmModeGetResources(drm_warpper->fd);
    drm_warpper->crtc_id = drm_warpper->res->crtcs[0];
    drm_warpper->conn_id = drm_warpper->res->connectors[0];
    
    drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
      log_error("failed to set client cap\n");
      return -1;
    }
    drm_warpper->plane_res = drmModeGetPlaneResources(drm_warpper->fd);
    log_info("Available Plane Count: %d", drm_warpper->plane_res->count_planes);

    drm_warpper->conn = drmModeGetConnector(drm_warpper->fd, drm_warpper->conn_id);

    log_info("Connector Name: %s, %dx%d, Refresh Rate: %d",
        drm_warpper->conn->modes[0].name, drm_warpper->conn->modes[0].vdisplay, drm_warpper->conn->modes[0].hdisplay,
        drm_warpper->conn->modes[0].vrefresh);

    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;

    drm_warpper_reset_cache_ioctl(drm_warpper);

    drm_warpper->thread_running = true;
    pthread_create(&drm_warpper->display_thread, NULL, drm_warpper_display_thread, drm_warpper);
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    drmModeFreeConnector(drm_warpper->conn);
    drmModeFreePlaneResources(drm_warpper->plane_res);
    drmModeFreeResources(drm_warpper->res);
    close(drm_warpper->fd);
    drm_warpper->thread_running = false;
    for(int i = 0; i < 4; i++){
        drm_warpper_destroy_layer(drm_warpper, i);
    }
    log_info("wait for display thread to finish");
    pthread_join(drm_warpper->display_thread, NULL);
    log_info("display thread finished");


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

int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    int ret;
    layer_t* layer = &drm_warpper->layer[layer_id];
    buf->width = layer->width;
    buf->height = layer->height;
    ret = drm_warpper_create_buffer_object(drm_warpper->fd, buf, layer->width, layer->height, layer->mode);
    if(ret < 0){
        log_error("failed to allocate buffer");
        return -1;
    }
    return 0;
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



int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    int ret;
    ret = drmModeSetPlane(drm_warpper->fd, 
        drm_warpper->plane_res->planes[layer_id], 
        drm_warpper->crtc_id, 
        buf->fb_id, 
        0,
        x, y, 
        buf->width, buf->height, 
        0, 0,
        (buf->width) << 16, (buf->height) << 16
    );
    if (ret < 0)
        log_error("drmModeSetPlane err %d", ret);
    return 0;
}
