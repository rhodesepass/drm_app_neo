#include "drm_warpper.h"
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <unistd.h>
#include "log.h"
#include "srgn_drm.h"
#include "config.h"

static inline int DRM_IOCTL(int fd, unsigned long cmd, void *arg) {
  int ret = drmIoctl(fd, cmd, arg);
  return ret < 0 ? -errno : ret;
}

static void drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;
    if (drmWaitVBlank(drm_warpper->fd, (drmVBlankPtr) &drm_warpper->blank)) {
      log_error("drmWaitVBlank failed");
    }
}

static void drm_warpper_switch_buffer_ioctl(drm_warpper_t *drm_warpper,int layer_id,int type,uint8_t *ch0_addr,uint8_t *ch1_addr,uint8_t *ch2_addr){
    int ret;
    struct drm_srgn_mount_fb srgn_mount_fb;

    srgn_mount_fb.layer_id = layer_id;
    srgn_mount_fb.type = type;
    srgn_mount_fb.ch0_addr = (uint32_t)ch0_addr;
    srgn_mount_fb.ch1_addr = (uint32_t)ch1_addr;
    srgn_mount_fb.ch2_addr = (uint32_t)ch2_addr;
    
    ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_MOUNT_FB, &srgn_mount_fb);
    if(ret < 0){
        log_error("drm_warpper_switch_buffer_ioctl failed %s(%d)", strerror(errno), errno);
    }
}

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    bool need_flip = false;
    int ret,i;

    log_info("display thread started");
    while(drm_warpper->thread_running){
        drm_warpper_wait_for_vsync(drm_warpper);

        for(int layer_id = 0; layer_id < 4; layer_id++){
            if(drm_warpper->plane[layer_id].used){
                buffer_object_t* buf;
                bool res = try_dequeue(&drm_warpper->plane[layer_id].ready_queue, &buf);
                if(res){
                    if(layer_id == DRM_WARPPER_LAYER_UI){
                        drm_warpper_switch_buffer_ioctl(
                            drm_warpper, 
                            layer_id, 
                            DRM_SRGN_MOUNT_FB_TYPE_NORMAL, 
                            buf->vaddr, 
                            NULL, 
                            NULL
                        );
                    }
                }
                for(int i = 0; i < 2; i++){
                    if(&drm_warpper->plane[layer_id].buf[i] != buf){
                        drm_warpper->plane[layer_id].buf[i].state = DRM_WARPPER_BUFFER_STATE_FREE;
                        sem_post(drm_warpper->plane[layer_id].buf[i].related_free_sem);
                    }
                }
            }
        }
    }
    return NULL;
}

int drm_warpper_arquire_draw_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr){
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    for(int i = 0; i < 2; i++){
        if(drm_warpper->plane[layer_id].buf[i].state == DRM_WARPPER_BUFFER_STATE_FREE){
            *vaddr = drm_warpper->plane[layer_id].buf[i].vaddr;
            drm_warpper->plane[layer_id].buf[i].state = DRM_WARPPER_BUFFER_STATE_DRAWING;
            return 0;
        }
    }
    log_error("failed to acquire draw buffer, but free semaphore is not 0?");
    return -1;
}

int drm_warpper_return_draw_buffer(drm_warpper_t *drm_warpper,int layer_id, uint8_t* vaddr){
    buffer_object_t *buf;

    for(int i = 0; i < 2; i++){
        buf = &drm_warpper->plane[layer_id].buf[i];
        if(buf->vaddr == vaddr){
            buf->state = DRM_WARPPER_BUFFER_STATE_READY;
            enqueue(&drm_warpper->plane[layer_id].ready_queue, &buf);
            return 0;
        }
    }
    log_error("failed to return draw buffer, but vaddr is not found?");
    return -1;
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
    pthread_join(drm_warpper->display_thread, NULL);
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

    plane_t* plane = &drm_warpper->plane[layer_id];

    plane->buf[0].width = width;
    plane->buf[0].height = height;
    plane->buf[1].width = width;
    plane->buf[1].height = height;
    
    drm_warpper_create_buffer_object(drm_warpper->fd, &plane->buf[0], width, height, mode);
    drm_warpper_create_buffer_object(drm_warpper->fd, &plane->buf[1], width, height, mode);

    sem_init(&plane->free_sem, 0, 2);
    plane->buf[0].related_free_sem = &plane->free_sem;
    plane->buf[1].related_free_sem = &plane->free_sem;
    plane->used = true;

    memset(&plane->ready_queue, 0, sizeof(queue_t));

    return 0;
}


int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    struct drm_mode_destroy_dumb destroy;

    memset(&destroy, 0, sizeof(struct drm_mode_destroy_dumb));

    drmModeRmFB(drm_warpper->fd, drm_warpper->plane[layer_id].buf[0].fb_id);
    munmap(drm_warpper->plane[layer_id].buf[0].vaddr, drm_warpper->plane[layer_id].buf[0].size);

    destroy.handle = drm_warpper->plane[layer_id].buf[0].handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    drmModeRmFB(drm_warpper->fd, drm_warpper->plane[layer_id].buf[1].fb_id);
    munmap(drm_warpper->plane[layer_id].buf[1].vaddr, drm_warpper->plane[layer_id].buf[1].size);

    destroy.handle = drm_warpper->plane[layer_id].buf[1].handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    sem_destroy(&drm_warpper->plane[layer_id].free_sem);
    drm_warpper->plane[layer_id].used = false;

    return 0;
}

int drm_warpper_get_layer_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr){
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    for(int i = 0; i < 2; i++){
        if(drm_warpper->plane[layer_id].buf[i].state == DRM_WARPPER_BUFFER_STATE_FREE){
            *vaddr = drm_warpper->plane[layer_id].buf[i].vaddr;
            drm_warpper->plane[layer_id].buf[i].state = DRM_WARPPER_BUFFER_STATE_DRAWING;
            return 0;
        }
    }
    log_error("failed to get layer buffer, but free semaphore is not 0?");
    return -1;
}

int drm_warpper_return_layer_buffer(drm_warpper_t *drm_warpper,int layer_id,int buf_id){
    drm_warpper->plane[layer_id].buf[buf_id].state = DRM_WARPPER_BUFFER_STATE_READY;
    enqueue(&drm_warpper->plane[layer_id].ready_queue, &buf_id);
    return 0;
}


int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    int ret;
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    drm_warpper->plane[layer_id].buf[0].state = DRM_WARPPER_BUFFER_STATE_DISPLAYING;
    ret = drmModeSetPlane(drm_warpper->fd, 
        drm_warpper->plane_res->planes[layer_id], 
        drm_warpper->crtc_id, 
        drm_warpper->plane[layer_id].buf[0].fb_id, 
        0,
        x, y, 
        drm_warpper->plane[layer_id].buf[0].width, drm_warpper->plane[layer_id].buf[0].height, 
        0, 0,
        (drm_warpper->plane[layer_id].buf[0].width) << 16, (drm_warpper->plane[layer_id].buf[0].height) << 16
    );
    if (ret < 0)
        log_error("drmModeSetPlane err %d", ret);
    return 0;
}


int drm_warpper_get_layer_buffer_legacy(drm_warpper_t *drm_warpper,int layer_id){
    return drm_warpper->plane[layer_id].buf[0].vaddr;
}
