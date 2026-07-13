/*
 * Mediaplayer 的 PC/ffmpeg 后端 —— 与 mediaplayer.c（设备：自制解码栈 + cedrus）
 * 二选一编译，公开 API 与行为契约逐条对齐：
 *   - play_video/start 不自动 stop、起线程即返回、单文件无限循环（EOF 回绕）
 *   - stop 阻塞到 join + 帧回流 + disable_layer_sync
 *   - 按 frame_duration_us 定速出帧（先备货再睡档期的简化版：软解无重排延迟）
 *   - frame_width/height 取 coded 尺寸（MB 对齐，与设备 SPS 报告值一致），
 *     沿用 main.c 的 video_layer_ensure_mount 白名单与裁窗几何
 *   - FLIP_FB item 的 userdata/session_gen 回收协议逐字复刻
 *
 * 帧池：drm_warpper_allocate_buffer_sized(VIDEO) 自建 N 格 planar NV12
 * （SDL 后端下即 malloc + fb_id 登记），sws_scale 从解码帧转 NV12 进池格。
 */

#include "mediaplayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "config.h"
#include "utils/misc.h"

#define mp_get_now_us get_now_us

#define MP_FF_POOL_COUNT 4

// main.c 提供，按解码尺寸记录 video 层挂载几何
extern int video_layer_ensure_mount(int src_w, int src_h);

typedef struct {
    AVFormatContext *fmt;
    AVCodecContext  *dec;
    AVFrame         *frame;
    AVPacket        *pkt;
    struct SwsContext *sws;
    int              sws_src_fmt;
    int              stream_idx;

    buffer_object_t  pool[MP_FF_POOL_COUNT];
    bool             slot_busy[MP_FF_POOL_COUNT]; // 已入队未从 free_queue 回流
} mp_ff_priv_t;

static bool mp_size_supported(int w, int h)
{
    if (w == VIDEO_WIDTH && h == VIDEO_HEIGHT)
        return true;
#ifdef VIDEO_LEGACY_WIDTH
    if (w == VIDEO_LEGACY_WIDTH && h == VIDEO_LEGACY_HEIGHT)
        return true;
#endif
#ifdef VIDEO_HIRES_WIDTH
    if (w == VIDEO_HIRES_WIDTH && h == VIDEO_HIRES_HEIGHT)
        return true;
#endif
    return false;
}

/* userdata 编码同设备：低 8 位 = slot+1，高位 = 会话代号 */
static inline void *slot_to_userdata(mediaplayer_t *mp, int slot)
{
    return (void *)(uintptr_t)(((uintptr_t)mp->session_gen << 8) |
                               (uint32_t)(slot + 1));
}

static void mp_reclaim_free_items(mediaplayer_t *mp)
{
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;
    drm_warpper_queue_item_t *item;

    while (drm_warpper_try_dequeue_free_item(mp->drm_warpper,
                                             DRM_WARPPER_LAYER_VIDEO,
                                             &item) == 0) {
        uintptr_t ud = (uintptr_t)item->userdata;
        int slot = (int)(ud & 0xff) - 1;

        if (slot >= 0 && slot < MP_FF_POOL_COUNT && (ud >> 8) == mp->session_gen)
            p->slot_busy[slot] = false;
        mp->items_in_flight--;
        free(item);
    }
}

static int mp_enqueue_frame(mediaplayer_t *mp, uint32_t fb_id, int slot)
{
    drm_warpper_queue_item_t *item = malloc(sizeof(*item));

    if (!item) {
        log_error("malloc err");
        return -1;
    }
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_FLIP_FB;
    item->fb_id = fb_id;
    item->userdata = slot_to_userdata(mp, slot);
    item->on_heap = false; /* 帧类 item 由本模块经 free_queue 回收 */

    mp->items_in_flight++;
    return drm_warpper_enqueue_display_item(mp->drm_warpper,
                                            DRM_WARPPER_LAYER_VIDEO, item);
}

/* 解出下一帧到 p->frame。0 成功；-1 错误。EOF 自动回绕（无限循环）。 */
static int mp_ff_next_frame(mediaplayer_t *mp)
{
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;

    while (1) {
        int rc = avcodec_receive_frame(p->dec, p->frame);
        if (rc == 0)
            return 0;
        if (rc != AVERROR(EAGAIN) && rc != AVERROR_EOF) {
            log_error("avcodec_receive_frame: %d", rc);
            return -1;
        }

        rc = av_read_frame(p->fmt, p->pkt);
        if (rc < 0) {
            /* EOS：回到文件头无限循环（同设备 sample 表回绕语义） */
            if (av_seek_frame(p->fmt, p->stream_idx, 0, AVSEEK_FLAG_BACKWARD) < 0) {
                log_error("loop seek failed");
                return -1;
            }
            avcodec_flush_buffers(p->dec);
            continue;
        }
        if (p->pkt->stream_index != p->stream_idx) {
            av_packet_unref(p->pkt);
            continue;
        }
        rc = avcodec_send_packet(p->dec, p->pkt);
        av_packet_unref(p->pkt);
        if (rc < 0 && rc != AVERROR(EAGAIN)) {
            log_error("avcodec_send_packet: %d", rc);
            return -1;
        }
    }
}

/* 把解码帧转成 NV12 写进池格 */
static int mp_ff_blit_to_slot(mediaplayer_t *mp, int slot)
{
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;
    buffer_object_t *buf = &p->pool[slot];
    AVFrame *f = p->frame;

    if (!p->sws || p->sws_src_fmt != f->format) {
        if (p->sws)
            sws_freeContext(p->sws);
        p->sws = sws_getContext(f->width, f->height, (enum AVPixelFormat)f->format,
                                f->width, f->height, AV_PIX_FMT_NV12,
                                SWS_BILINEAR, NULL, NULL, NULL);
        p->sws_src_fmt = f->format;
        if (!p->sws) {
            log_error("sws_getContext failed (fmt=%d)", f->format);
            return -1;
        }
    }

    /* 帧显示宽可能小于池格的 coded 宽（对齐 padding），padding 列在
     * 分配时已刷黑且被 video 层的 src 裁窗排除 */
    int uv_offset = (int)buf->pitch * (int)buf->height;
    uint8_t *dst_data[4] = { buf->vaddr, buf->vaddr + uv_offset, NULL, NULL };
    int dst_linesize[4] = { (int)buf->pitch, (int)buf->pitch, 0, 0 };

    sws_scale(p->sws, (const uint8_t * const *)f->data, f->linesize,
              0, f->height, dst_data, dst_linesize);
    return 0;
}

static void *mp_decode_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;
    unsigned int outputs = 0;
    long long next_frame_time;

    log_info("==> mp_decode(ffmpeg) Thread Started! dur=%uus", mp->frame_duration_us);

    next_frame_time = mp_get_now_us() + mp->frame_duration_us;

    while (1) {
        long long now;
        int slot, retry;

        pthread_rwlock_rdlock(&mp->thread.rwlock);
        int requested_stop = mp->thread.requested_stop;
        pthread_rwlock_unlock(&mp->thread.rwlock);
        if (requested_stop)
            break;

        mp_reclaim_free_items(mp);

        /* 找空池格：无空格 = 在飞帧太多，等显示线程回流 */
        slot = -1;
        for (retry = 0; retry < 100 && slot < 0; retry++) {
            for (int i = 0; i < MP_FF_POOL_COUNT; i++) {
                if (!p->slot_busy[i]) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) {
                usleep(5 * 1000);
                mp_reclaim_free_items(mp);
            }
        }
        if (slot < 0) {
            log_error("no free frame slot");
            goto decode_error;
        }

        /* 先备货再睡档期（软解无 DPB 重排延迟，直接解一帧） */
        if (mp_ff_next_frame(mp) < 0)
            goto decode_error;
        if (mp_ff_blit_to_slot(mp, slot) < 0)
            goto decode_error;
        av_frame_unref(p->frame);

        now = mp_get_now_us();
        if (now < next_frame_time)
            usleep(next_frame_time - now);
        else if (now > next_frame_time + 2 * 1000 * 1000) {
            log_warn("can't keep up, delay: %lld us", now - next_frame_time);
            next_frame_time = now;
        }

        mp_reclaim_free_items(mp);
        p->slot_busy[slot] = true;
        mp_enqueue_frame(mp, p->pool[slot].fb_id, slot);
        next_frame_time += mp->frame_duration_us;
        if (++outputs % 300 == 0)
            log_info("mp pace: out=%u lag=%lldms", outputs,
                     (long long)(int64_t)(mp_get_now_us() - next_frame_time) / 1000);
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    log_info("==> mp_decode(ffmpeg) Thread Ended!");
    pthread_exit(NULL);
    return NULL;

decode_error:
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_DECODER_ERROR | MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    log_error("==> mp_decode(ffmpeg) Thread Ended (error)!");
    pthread_exit(NULL);
    return NULL;
}

static void mp_close_session(mediaplayer_t *mp)
{
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;

    if (!mp->session_open)
        return;

    for (int i = 0; i < MP_FF_POOL_COUNT; i++) {
        if (p->pool[i].vaddr) {
            drm_warpper_free_buffer(mp->drm_warpper, DRM_WARPPER_LAYER_VIDEO,
                                    &p->pool[i]);
        }
        p->slot_busy[i] = false;
    }
    if (p->sws) {
        sws_freeContext(p->sws);
        p->sws = NULL;
    }
    if (p->frame) av_frame_free(&p->frame);
    if (p->pkt) av_packet_free(&p->pkt);
    if (p->dec) avcodec_free_context(&p->dec);
    if (p->fmt) avformat_close_input(&p->fmt);
    mp->session_open = false;
}

int mediaplayer_init(mediaplayer_t *mp, drm_warpper_t *drm_warpper)
{
    memset(mp, 0, sizeof(*mp));

    mp->priv = calloc(1, sizeof(mp_ff_priv_t));
    if (!mp->priv) {
        log_error("mediaplayer priv alloc failed");
        return -1;
    }
    pthread_rwlock_init(&mp->thread.rwlock, NULL);
    atomic_store(&mp->running, 0);
    mp->drm_warpper = drm_warpper;

    log_info("==> mp (ffmpeg backend) Initalized!");
    return 0;
}

int mediaplayer_destroy(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }

    mediaplayer_stop(mp);
    pthread_rwlock_destroy(&mp->thread.rwlock);
    free(mp->priv);
    mp->priv = NULL;

    return 0;
}

int mediaplayer_remount_video_layer(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }
    int w = mp->frame_width  ? mp->frame_width  : VIDEO_WIDTH;
    int h = mp->frame_height ? mp->frame_height : VIDEO_HEIGHT;
    return video_layer_ensure_mount(w, h);
}

static int mp_prepare_and_spawn(mediaplayer_t *mp)
{
    mp_ff_priv_t *p = (mp_ff_priv_t *)mp->priv;
    const AVCodec *codec = NULL;
    AVStream *st;
    AVRational fr;

    if (avformat_open_input(&p->fmt, mp->input_path, NULL, NULL) < 0) {
        log_error("avformat_open_input err: %s", mp->input_path);
        return -1;
    }
    mp->session_gen++;
    mp->session_open = true;

    if (avformat_find_stream_info(p->fmt, NULL) < 0) {
        log_error("find_stream_info err");
        goto error;
    }
    p->stream_idx = av_find_best_stream(p->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (p->stream_idx < 0 || !codec) {
        log_error("no video stream");
        goto error;
    }
    st = p->fmt->streams[p->stream_idx];

    p->dec = avcodec_alloc_context3(codec);
    if (!p->dec)
        goto error;
    if (avcodec_parameters_to_context(p->dec, st->codecpar) < 0 ||
        avcodec_open2(p->dec, codec, NULL) < 0) {
        log_error("codec open err");
        goto error;
    }

    /* coded 尺寸(MB 对齐)与设备 SPS 报告值一致(384x640/736x1280)；
     * 打开后 coded_* 才可信，缺省回退 display 尺寸 */
    mp->frame_width = p->dec->coded_width > 0 ? p->dec->coded_width : p->dec->width;
    mp->frame_height = p->dec->coded_height > 0 ? p->dec->coded_height : p->dec->height;

    if (!mp_size_supported(mp->frame_width, mp->frame_height)) {
        log_error("unsupported video size %dx%d", mp->frame_width, mp->frame_height);
        goto error;
    }
    video_layer_ensure_mount(mp->frame_width, mp->frame_height);

    fr = av_guess_frame_rate(p->fmt, st, NULL);
    if (fr.num > 0 && fr.den > 0)
        mp->frame_duration_us = (uint32_t)((int64_t)fr.den * 1000000 / fr.num);
    else
        mp->frame_duration_us = 33333;

    p->frame = av_frame_alloc();
    p->pkt = av_packet_alloc();
    if (!p->frame || !p->pkt)
        goto error;
    p->sws = NULL;
    p->sws_src_fmt = -1;

    for (int i = 0; i < MP_FF_POOL_COUNT; i++) {
        if (drm_warpper_allocate_buffer_sized(mp->drm_warpper,
                                              DRM_WARPPER_LAYER_VIDEO,
                                              mp->frame_width, mp->frame_height,
                                              &p->pool[i]) != 0) {
            log_error("frame pool alloc err");
            goto error;
        }
        p->slot_busy[i] = false;
    }

    log_info("ffmpeg: %s %dx%d dur=%uus codec=%s",
             mp->input_path, mp->frame_width, mp->frame_height,
             mp->frame_duration_us, codec->name);

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    atomic_store(&mp->running, 1);

    if (pthread_create(&mp->decode_thread, NULL, mp_decode_thread, mp) != 0) {
        log_error("decode thread create err");
        atomic_store(&mp->running, 0);
        goto error;
    }

    return 0;

error:
    mp_close_session(mp);
    return -1;
}

int mediaplayer_play_video(mediaplayer_t *mp, const char *file)
{
    if (!mp || !file) {
        log_error("invalid params");
        return -1;
    }

    if (atomic_load(&mp->running)) {
        log_error("mediaplayer is running");
        return -1;
    }

    snprintf(mp->input_path, sizeof(mp->input_path), "%s", file);

    return mp_prepare_and_spawn(mp);
}

int mediaplayer_stop(mediaplayer_t *mp)
{
    int wait;

    if (!mp) {
        return -1;
    }

    if (!atomic_load(&mp->running)) {
        return 0;
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.requested_stop = 1;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    pthread_join(mp->decode_thread, NULL);
    atomic_store(&mp->running, 0);

    // 等积压的 FLIP 回流（SDL 后端翻页即拷贝并归还，通常一个合成周期内清零）
    for (wait = 0; wait < 40 && mp->items_in_flight > 0; wait++) {
        usleep(10 * 1000);
        mp_reclaim_free_items(mp);
    }
    if (mp->items_in_flight > 0)
        log_warn("stop: %d frame items still in flight", mp->items_in_flight);

    drm_warpper_disable_layer_sync(mp->drm_warpper, DRM_WARPPER_LAYER_VIDEO);

    mp_close_session(mp);

    return 0;
}

int mediaplayer_set_video(mediaplayer_t *mp, const char *path)
{
    if (!mp || !path) {
        log_error("invalid params");
        return -1;
    }

    if (atomic_load(&mp->running)) {
        log_error("cannot set video while playing, stop first");
        return -1;
    }

    snprintf(mp->video_path, sizeof(mp->video_path), "%s", path);
    log_info("video path set to: %s", mp->video_path);

    return 0;
}

int mediaplayer_start(mediaplayer_t *mp)
{
    if (!mp) {
        log_error("invalid params");
        return -1;
    }

    if (atomic_load(&mp->running)) {
        log_warn("mediaplayer already running");
        return 0;
    }

    if (strlen(mp->video_path) == 0) {
        log_error("no video path set");
        return -1;
    }

    snprintf(mp->input_path, sizeof(mp->input_path), "%s", mp->video_path);

    int ret = mp_prepare_and_spawn(mp);
    if (ret != 0) {
        return ret;
    }

    log_info("playback started");
    return 0;
}

mp_status_t mediaplayer_get_status(mediaplayer_t *mp)
{
    if (!mp) {
        return MP_STATUS_ERROR;
    }

    if (!atomic_load(&mp->running)) {
        return MP_STATUS_STOPPED;
    }

    return MP_STATUS_PLAYING;
}
