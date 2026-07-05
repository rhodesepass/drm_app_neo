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
#include <poll.h>
#include <time.h>
#include <unistd.h>

#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "config.h"
#include "utils/spsc_queue.h"

static void drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;
    if (drmWaitVBlank(drm_warpper->fd, (drmVBlankPtr) &drm_warpper->blank)) {
      log_error("drmWaitVBlank failed %s(%d)", strerror(errno), errno);
      usleep(20 * 1000); // 失败也维持节奏，防止立即返回时空转烧 CPU
    }
}

// alpha 属性 0..0xFFFF；255*0x101 = 0xFFFF 正好回到不透明(像素 alpha)
static inline uint64_t alpha_to_prop(uint8_t alpha){
    return (uint64_t)alpha * 0x101;
}

static uint32_t drm_warpper_find_prop(int fd, uint32_t obj_id, uint32_t obj_type, const char *name){
    drmModeObjectProperties *props;
    uint32_t id = 0;
    int i;

    props = drmModeObjectGetProperties(fd, obj_id, obj_type);
    if (!props)
        return 0;
    for (i = 0; i < (int)props->count_props && !id; i++) {
        drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
        if (!prop)
            continue;
        if (!strcmp(prop->name, name))
            id = prop->prop_id;
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);
    return id;
}

// 内核关掉 DRM_FBDEV_EMULATION 后没有 fbcon 替我们做开机 modeset，
// CRTC 是灭的：plane commit 全被拒、drmWaitVBlank 每次卡 3s 超时。
// 检测到 CRTC 未点亮就自己做一次(mode 取 connector 首选模式)。
static int drm_warpper_ensure_crtc_active(drm_warpper_t *drm_warpper){
    drmModeCrtc *crtc;
    drmModeAtomicReq *req;
    uint32_t conn_crtc_prop, crtc_mode_prop, crtc_active_prop, blob_id;
    bool active;
    int ret;

    crtc = drmModeGetCrtc(drm_warpper->fd, drm_warpper->crtc_id);
    active = crtc && crtc->mode_valid;
    drmModeFreeCrtc(crtc);
    if (active)
        return 0;

    log_info("CRTC is off (no fbcon modeset), performing initial modeset");

    conn_crtc_prop = drm_warpper_find_prop(drm_warpper->fd, drm_warpper->conn_id,
                                           DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");
    crtc_mode_prop = drm_warpper_find_prop(drm_warpper->fd, drm_warpper->crtc_id,
                                           DRM_MODE_OBJECT_CRTC, "MODE_ID");
    crtc_active_prop = drm_warpper_find_prop(drm_warpper->fd, drm_warpper->crtc_id,
                                             DRM_MODE_OBJECT_CRTC, "ACTIVE");
    if (!conn_crtc_prop || !crtc_mode_prop || !crtc_active_prop) {
        log_error("modeset properties missing");
        return -1;
    }

    ret = drmModeCreatePropertyBlob(drm_warpper->fd, &drm_warpper->conn->modes[0],
                                    sizeof(drm_warpper->conn->modes[0]), &blob_id);
    if (ret < 0) {
        log_error("create mode blob failed %s(%d)", strerror(errno), errno);
        return -1;
    }

    req = drmModeAtomicAlloc();
    if (!req)
        return -1;
    drmModeAtomicAddProperty(req, drm_warpper->conn_id, conn_crtc_prop, drm_warpper->crtc_id);
    drmModeAtomicAddProperty(req, drm_warpper->crtc_id, crtc_mode_prop, blob_id);
    drmModeAtomicAddProperty(req, drm_warpper->crtc_id, crtc_active_prop, 1);

    ret = drmModeAtomicCommit(drm_warpper->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    drmModeAtomicFree(req);
    if (ret < 0)
        log_error("initial modeset commit err %s(%d)", strerror(errno), errno);
    return ret;
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

// 每层每轮 drain 到的帧类 item。free_queue 回收必须推迟到 commit 返回之后：
// 提前回收的话，解码侧会在旧帧还在扫描时就把该 capture buffer 重新 QBUF
// 给 cedrus 覆写——屏上直接花。
#define DRM_WARPPER_DRAIN_MAX 16
typedef struct {
    drm_warpper_queue_item_t *items[DRM_WARPPER_DRAIN_MAX];
    int n;
} drained_frames_t;

static inline int64_t dw_now_us(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// flip event = pending 帧已被硬件 latch(正在扫描)，被换下的 curr 离屏，
// 此刻回收才安全。事件只由显示线程消费(其他线程不 read drm fd)。
static void dw_flip_handler(int fd, unsigned int sequence,
                            unsigned int tv_sec, unsigned int tv_usec,
                            unsigned int crtc_id, void *user_data){
    layer_t *layer = (layer_t *)user_data;
    (void)fd; (void)sequence; (void)tv_sec; (void)tv_usec; (void)crtc_id;
    if(!layer || !layer->pending_item)
        return;
    if(layer->curr_item)
        spsc_bq_push(&layer->free_queue, layer->curr_item);
    layer->curr_item = layer->pending_item;
    layer->pending_item = NULL;
}

// 等 pending 翻页完成。timeout_ms=0 即"有事件就收，没有不等"。
// 超时(事件丢失，不应发生)按已 latch 强制推进，避免卡死或漏回收。
static void dw_reap_flip(drm_warpper_t *drm_warpper, layer_t *layer, int timeout_ms){
    drmEventContext ev = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler2 = dw_flip_handler,
    };
    struct pollfd pfd = { .fd = drm_warpper->fd, .events = POLLIN };

    while(layer->pending_item){
        int pr = poll(&pfd, 1, timeout_ms);
        if(pr > 0){
            drmHandleEvent(drm_warpper->fd, &ev);
            continue;
        }
        if(pr == 0 && timeout_ms > 0)
            log_error("flip event lost, force-completing");
        if(timeout_ms > 0)
            dw_flip_handler(drm_warpper->fd, 0, 0, 0, 0, layer);
        break;
    }
}

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    int ret;
    // video 层节拍诊断：commit 间隔与跳帧/空转分布，量化"抖动"
    struct {
        uint32_t commits, frames, empties;
        int64_t last_us, sum_us, max_us;
    } vstat = { 0 };

    log_info("==> DRM_Warpper Display Thread Started!");

    // 节奏：有活时直接阻塞 commit（commit 本身按 vblank 节拍返回，
    // 背靠背即每 vblank 一次）；空转时用 drmWaitVBlank 兜底等待。
    while(atomic_load(&drm_warpper->thread_running)){
        drmModeAtomicReq *req = NULL;
        drained_frames_t drained[4] = { 0 };

        // 顺手收上一次翻页的完成事件(不等待)，让离屏帧尽快回流
        dw_reap_flip(drm_warpper, &drm_warpper->layer[DRM_WARPPER_LAYER_VIDEO], 0);

        for(int i = 0; i < 4; i++){
            layer_t* layer = &drm_warpper->layer[i];
            plane_prop_ids_t *p = &drm_warpper->plane_props[i];
            if(!layer->used)
                continue;
            drm_warpper_queue_item_t* item;
            while(spsc_bq_try_pop(&layer->display_queue, (void**)&item) == 0){
                switch(item->type){
                case DRM_WARPPER_ITEM_FLIP_FB:
                    if(drained[i].n < DRM_WARPPER_DRAIN_MAX){
                        drained[i].items[drained[i].n++] = item;
                    }
                    else{
                        // 不可能路径(display_queue 容量即 16)，直接判为已跳过
                        spsc_bq_push(&layer->free_queue, item);
                    }
                    break;
                case DRM_WARPPER_ITEM_SET_COORD:
                    if(!req) req = drmModeAtomicAlloc();
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->crtc_x, (uint64_t)(int64_t)item->x);
                    drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                             p->crtc_y, (uint64_t)(int64_t)item->y);
                    if(item->on_heap) free(item);
                    break;
                case DRM_WARPPER_ITEM_SET_ALPHA:
                    if(p->alpha){
                        if(!req) req = drmModeAtomicAlloc();
                        drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i],
                                                 p->alpha, alpha_to_prop(item->alpha));
                    }
                    else{
                        log_error("layer %d has no alpha property", i);
                    }
                    if(item->on_heap) free(item);
                    break;
                }
            }

            // 多个待翻帧只上最后一帧(60fps 素材 vs 40Hz 屏，跳帧在此发生)
            if(drained[i].n > 0){
                if(!req) req = drmModeAtomicAlloc();
                drmModeAtomicAddProperty(req, drm_warpper->plane_ids[i], p->fb_id,
                                         drained[i].items[drained[i].n - 1]->fb_id);
                // 惰性挂载：plane 处于 disabled 时随首帧一并启用
                if(layer->needs_full_mount){
                    uint32_t pl = drm_warpper->plane_ids[i];
                    drmModeAtomicAddProperty(req, pl, p->crtc_id, drm_warpper->crtc_id);
                    drmModeAtomicAddProperty(req, pl, p->src_x, 0);
                    drmModeAtomicAddProperty(req, pl, p->src_y, 0);
                    drmModeAtomicAddProperty(req, pl, p->src_w, (uint64_t)layer->geo_src_w << 16);
                    drmModeAtomicAddProperty(req, pl, p->src_h, (uint64_t)layer->geo_src_h << 16);
                    drmModeAtomicAddProperty(req, pl, p->crtc_x, (uint64_t)(int64_t)layer->geo_x);
                    drmModeAtomicAddProperty(req, pl, p->crtc_y, (uint64_t)(int64_t)layer->geo_y);
                    drmModeAtomicAddProperty(req, pl, p->crtc_w, layer->geo_dst_w);
                    drmModeAtomicAddProperty(req, pl, p->crtc_h, layer->geo_dst_h);
                }
            }
        }

        if(req){
            layer_t *vlayer = &drm_warpper->layer[DRM_WARPPER_LAYER_VIDEO];
            bool has_video = drained[DRM_WARPPER_LAYER_VIDEO].n > 0;
            int64_t c0, cdt;
            uint32_t flags = 0;
            void *udata = NULL;

            // BSP 式非阻塞翻页：commit 只递交，硬件在下一个 vblank 用
            // shadow-load latch，节拍不再被"等 vblank"的墙钟卡住。
            // 单发在飞：上一发未完成先收其事件(避免 -EBUSY)
            if(has_video){
                dw_reap_flip(drm_warpper, vlayer, 200);
                // CRTC_ID 每发都带：把 crtc state 拉进 commit，
                // PAGE_FLIP_EVENT 才会挂到该 CRTC 上产生完成事件
                drmModeAtomicAddProperty(req,
                                         drm_warpper->plane_ids[DRM_WARPPER_LAYER_VIDEO],
                                         drm_warpper->plane_props[DRM_WARPPER_LAYER_VIDEO].crtc_id,
                                         drm_warpper->crtc_id);
                flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
                udata = vlayer;
            }

            c0 = dw_now_us();
            pthread_mutex_lock(&drm_warpper->commit_mutex);
            ret = drmModeAtomicCommit(drm_warpper->fd, req, flags, udata);
            pthread_mutex_unlock(&drm_warpper->commit_mutex);
            cdt = dw_now_us() - c0;
            if(cdt > 60000)
                log_warn("slow commit %lldus (video frames=%d)",
                         (long long)cdt, drained[DRM_WARPPER_LAYER_VIDEO].n);
            if(ret < 0){
                log_error("drmModeAtomicCommit failed %s(%d)", strerror(errno), errno);
            }
            drmModeAtomicFree(req);

            if(ret == 0 && has_video){
                static int dw_trace = -1;
                if(dw_trace < 0)
                    dw_trace = getenv("MP_TRACE") != NULL;
                if(dw_trace){
                    drained_frames_t *dv = &drained[DRM_WARPPER_LAYER_VIDEO];
                    log_info("T C%d skip%d",
                             (int)(((uintptr_t)dv->items[dv->n - 1]->userdata) & 0xff) - 1,
                             dv->n - 1);
                }
                int64_t now = dw_now_us();
                if(vstat.last_us){
                    int64_t dt = now - vstat.last_us;
                    vstat.sum_us += dt;
                    if(dt > vstat.max_us) vstat.max_us = dt;
                }
                vstat.last_us = now;
                vstat.commits++;
                vstat.frames += drained[DRM_WARPPER_LAYER_VIDEO].n;
                if(vstat.commits == 400){
                    log_info("vstat: 400 commits %u frames(skip %u) empty=%u avg=%lldus max=%lldus",
                             vstat.frames, vstat.frames - vstat.commits,
                             vstat.empties, (long long)vstat.sum_us / 399,
                             (long long)vstat.max_us);
                    memset(&vstat, 0, sizeof(vstat));
                    vstat.last_us = now;
                }
            }

            for(int i = 0; i < 4; i++){
                layer_t* layer = &drm_warpper->layer[i];
                int n = drained[i].n;
                if(n == 0)
                    continue;
                if(ret == 0){
                    layer->needs_full_mount = false;
                    // 被跳过的 n-1 帧从未递交给硬件，立即回收
                    for(int k = 0; k < n - 1; k++)
                        spsc_bq_push(&layer->free_queue, drained[i].items[k]);
                    if(i == DRM_WARPPER_LAYER_VIDEO && has_video){
                        // 最后一帧在飞(pending)，flip event 到达才顶替 curr
                        layer->pending_item = drained[i].items[n - 1];
                    }
                    else{
                        // 非 video 层不带事件(实践中也不走帧翻页)：沿用
                        // curr 直换，旧 curr 立即回收
                        if(layer->curr_item)
                            spsc_bq_push(&layer->free_queue, layer->curr_item);
                        layer->curr_item = drained[i].items[n - 1];
                    }
                }
                else{
                    // commit 被拒：屏上还是旧 curr，本轮所有帧原样回收
                    for(int k = 0; k < n; k++)
                        spsc_bq_push(&layer->free_queue, drained[i].items[k]);
                }
            }
        }
        else{
            if(vstat.last_us) vstat.empties++;
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

    // 上一实例还没死透时 open 拿不到 master，之后所有 atomic 全 EACCES。
    // 短暂重试等旧实例放手；拿不到也继续跑(仅报警)，别整只 app 起不来
    for (int tries = 0; drmSetMaster(drm_warpper->fd) != 0; tries++) {
        if (tries >= 20) {
            log_error("cannot become DRM master: %s (stale instance alive?)",
                      strerror(errno));
            break;
        }
        usleep(100 * 1000);
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

    if (drm_warpper_ensure_crtc_active(drm_warpper) < 0) {
        log_error("CRTC bring-up failed, display will not work");
        drmModeFreeConnector(drm_warpper->conn);
        drmModeFreePlaneResources(drm_warpper->plane_res);
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }

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
    // 必须 ≥ 全部在飞帧 item 数(video 层 VDEC_CAPTURE_BUF_MAX_SMALL=16
    // 加跨会话残留的 curr/pending)：push 是阻塞语义，容量不足会让显示线程
    // 卡在 drain 中途
    ret = spsc_bq_init(&layer->free_queue, 20);
    if(ret < 0){
        log_error("failed to initialize free queue");
        return -1;
    }

    layer->mode = mode;
    layer->used = true;
    layer->width = width;
    layer->height = height;

    layer->curr_item = NULL;
    layer->pending_item = NULL;
    layer->needs_full_mount = false;

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

int drm_warpper_import_dmabuf_fb(drm_warpper_t *drm_warpper,int dmabuf_fd,int width,int height,int pitch,int uv_offset,uint32_t *fb_id,uint32_t *gem_handle){
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
        struct drm_gem_close gc = { .handle = handle };
        drmIoctl(drm_warpper->fd, DRM_IOCTL_GEM_CLOSE, &gc);
        return -1;
    }
    if(gem_handle)
        *gem_handle = handle;
    return 0;
}

int drm_warpper_rm_fb(drm_warpper_t *drm_warpper,uint32_t fb_id,uint32_t gem_handle){
    int ret = drmModeRmFB(drm_warpper->fd, fb_id);
    // prime 导入的 GEM handle 是独立引用，不 close 会一直 pin 住 dmabuf
    // 背后的 CMA——REQBUFS(0) 也还不回去，反复 play/stop 几次就把 CMA 吃光
    if(gem_handle){
        struct drm_gem_close gc = { .handle = gem_handle };
        drmIoctl(drm_warpper->fd, DRM_IOCTL_GEM_CLOSE, &gc);
    }
    return ret;
}

int drm_warpper_set_layer_geometry(drm_warpper_t *drm_warpper,int layer_id,int x,int y,int src_w,int src_h,int dst_w,int dst_h){
    layer_t* layer = &drm_warpper->layer[layer_id];

    layer->geo_x = (int16_t)x;
    layer->geo_y = (int16_t)y;
    layer->geo_src_w = src_w;
    layer->geo_src_h = src_h;
    layer->geo_dst_w = dst_w;
    layer->geo_dst_h = dst_h;
    layer->needs_full_mount = true;
    return 0;
}

int drm_warpper_disable_layer_sync(drm_warpper_t *drm_warpper,int layer_id){
    plane_prop_ids_t *p = &drm_warpper->plane_props[layer_id];
    uint32_t plane_id = drm_warpper->plane_ids[layer_id];
    drmModeAtomicReq *req;
    int ret;

    req = drmModeAtomicAlloc();
    if(!req)
        return -1;
    drmModeAtomicAddProperty(req, plane_id, p->crtc_id, 0);
    drmModeAtomicAddProperty(req, plane_id, p->fb_id, 0);

    pthread_mutex_lock(&drm_warpper->commit_mutex);
    ret = drmModeAtomicCommit(drm_warpper->fd, req, 0, NULL);
    pthread_mutex_unlock(&drm_warpper->commit_mutex);
    drmModeAtomicFree(req);

    if(ret < 0)
        log_error("disable plane commit err %s(%d)", strerror(errno), errno);
    else
        drm_warpper->layer[layer_id].needs_full_mount = true;
    return ret;
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

int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    return drm_warpper_mount_layer_rect(drm_warpper, layer_id, x, y, buf, buf->width, buf->height, buf->width, buf->height);
}
