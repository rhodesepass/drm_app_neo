/*
 * Mediaplayer：src/vdec 自制解码栈（MP4 demux + H264 parser/DPB + cedrus
 * V4L2 request API）→ dmabuf FB → drm_warpper atomic 翻页。
 *
 * 单解码线程：demux 是内存 sample 表、request 是同步等待，无重叠收益。
 * capture slot == DPB slot；帧经 FLIP_FB item 上屏，显示线程换帧后旧 item
 * 从 free_queue 回流，此时才解除该 slot 的 on_screen 占用。
 */

#include "mediaplayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "config.h"
#include "utils/misc.h"

#include "vdec/nalu.h"

#define mp_get_now_us get_now_us

// main.c 提供，按解码尺寸记录 video 层挂载几何(720 档旧素材走 DEFE 放大)
extern int video_layer_ensure_mount(int src_w, int src_h);

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

/*
 * userdata 编码：低 8 位 = slot+1(0 表示无槽位)，高位 = 会话代号。
 * stop 超时后旧会话 item 可能在下一会话才回流，代号不符时只回收
 * 计数，不能去碰新 DPB 的 on_screen(槽位号已是别人的了)。
 */
static inline void *slot_to_userdata(mediaplayer_t *mp, int slot)
{
    return (void *)(uintptr_t)(((uintptr_t)mp->session_gen << 8) |
                               (uint32_t)(slot + 1));
}

/* MP_TRACE=1 时打印槽位生命周期(D解码进槽/E入队/R回收，drm_warpper 侧 C上屏) */
static int mp_trace = -1;
static inline int mp_trace_on(void)
{
    if (mp_trace < 0)
        mp_trace = getenv("MP_TRACE") != NULL;
    return mp_trace;
}

/* 收 free_queue：解除离屏槽位的 on_screen 占用并释放 item */
static void mp_reclaim_free_items(mediaplayer_t *mp)
{
    drm_warpper_queue_item_t *item;

    while (drm_warpper_try_dequeue_free_item(mp->drm_warpper,
                                             DRM_WARPPER_LAYER_VIDEO,
                                             &item) == 0) {
        uintptr_t ud = (uintptr_t)item->userdata;
        int slot = (int)(ud & 0xff) - 1;

        if (mp_trace_on())
            log_info("T R%d g%d", slot, (int)((ud >> 8) == mp->session_gen));
        if (slot >= 0 && (ud >> 8) == mp->session_gen)
            h264_dpb_set_on_screen(&mp->dpb, slot, false);
        mp->items_in_flight--;
        free(item);
    }
}

/* 把 slot 的帧入队显示。返回 0 成功。 */
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

    if (slot >= 0) {
        h264_dpb_set_on_screen(&mp->dpb, slot, true);
        h264_dpb_mark_displayed(&mp->dpb, slot);
    }
    if (mp_trace_on())
        log_info("T E%d", slot);
    mp->items_in_flight++;
    return drm_warpper_enqueue_display_item(mp->drm_warpper,
                                            DRM_WARPPER_LAYER_VIDEO, item);
}

/*
 * 解码一个 AU（单 slice）。返回:
 *  0 解码成功  1 非视频帧(跳过)  -1 错误(停播)
 */
static int mp_decode_au(mediaplayer_t *mp, unsigned int sample_idx,
                        struct h264_slice_hdr *hdr_out)
{
    const struct mp4_sample *sample = &mp->demux.samples[sample_idx];
    const uint8_t *au = mp4_sample_data(&mp->demux, sample_idx);
    unsigned int cursor = 0, vcl_count = 0;
    struct nalu n, vcl = { 0 };
    struct h264_poc poc;
    struct vdec_h264_ctrls ctrl;
    bool have_hdr = false;
    uint64_t ts;
    int slot, retry;

    if (!au) {
        log_error("sample %u out of range", sample_idx);
        return -1;
    }

    while (nalu_next_length_prefixed(au, sample->size,
                                     mp->demux.nal_length_size, &cursor, &n)) {
        unsigned int t = nalu_h264_type(&n);

        if (t == H264_NAL_SPS || t == H264_NAL_PPS) {
            h264_parser_parse_param_nal(&mp->parser, &n);
        } else if (t == H264_NAL_SLICE || t == H264_NAL_IDR) {
            vcl_count++;
            if (have_hdr)
                continue;
            if (h264_parser_parse_slice(&mp->parser, &n, hdr_out) < 0) {
                log_error("slice parse failed @%u", sample_idx);
                return -1;
            }
            h264_parser_compute_poc(&mp->parser, hdr_out, &poc);
            vcl = n;
            have_hdr = true;
        }
    }

    if (!have_hdr)
        return 1;

    /* 素材管线保证单 slice/帧；多 slice 明确报错，不静默花屏 */
    if (vcl_count > 1) {
        log_error("frame %u has %u slices, unsupported", sample_idx, vcl_count);
        return -1;
    }

    if (h264_parser_fill_controls(&mp->parser, hdr_out, &poc, &ctrl) < 0) {
        log_error("fill_controls failed @%u", sample_idx);
        return -1;
    }

    long long t0 = mp_get_now_us(), t1, t2;

    /* 无空槽 = 在飞帧太多，等显示线程回流(每 vblank 一次) */
    for (retry = 0; retry < 100; retry++) {
        slot = h264_dpb_begin_frame(&mp->dpb, hdr_out, &poc, &ts, &ctrl);
        if (slot >= 0)
            break;
        usleep(5 * 1000);
        mp_reclaim_free_items(mp);
    }
    if (slot < 0) {
        log_error("no free capture slot @%u", sample_idx);
        return -1;
    }
    if (mp_trace_on())
        log_info("T D%d @%u", slot, sample_idx);
    t1 = mp_get_now_us();

    if (vdec_decode(&mp->vdec, slot, ts, vcl.data, vcl.size, &ctrl) < 0) {
        log_error("decode failed @%u", sample_idx);
        h264_dpb_abort_frame(&mp->dpb);
        return -1;
    }

    h264_dpb_end_frame(&mp->dpb, hdr_out);
    t2 = mp_get_now_us();
    if (t2 - t0 > 50000)
        log_warn("slow @%u: slot_wait=%lldus ve=%lldus size=%u",
                 sample_idx, t1 - t0, t2 - t1, vcl.size);
    return 0;
}

/* 解码线程：每 tick 出一帧（POC 序），按 frame_duration_us 定速 */
static void *mp_decode_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    unsigned int sample_idx = 0, outputs = 0;
    bool pending_flush = false;
    long long next_frame_time;
    int out;

    log_info("==> mp_decode Thread Started! dur=%uus samples=%u",
             mp->frame_duration_us, mp->demux.samples_count);

    next_frame_time = mp_get_now_us() + mp->frame_duration_us;

    while (1) {
        long long now;

        pthread_rwlock_rdlock(&mp->thread.rwlock);
        int requested_stop = mp->thread.requested_stop;
        pthread_rwlock_unlock(&mp->thread.rwlock);
        if (requested_stop)
            break;

        mp_reclaim_free_items(mp);

        /*
         * 先备货再睡档期：把"喂 AU 直到吐出一帧"放在睡眠窗口之前，
         * GOP 边界的重排回填(IDR 后要连喂 reorder 个 AU 才有输出)就
         * 消化在本来空转的等待里，不再把 IDR 帧拖过档期 ~2 个 VE 周期
         * (曾表现为每 GOP 一次 ~100ms 定格)。
         */

        /* GOP 边界(素材回绕)先按 flush 逐帧排空 DPB */
        if (pending_flush) {
            out = h264_dpb_next_output(&mp->dpb, true);
            if (out < 0)
                pending_flush = false;
        } else {
            out = h264_dpb_next_output(&mp->dpb, false);
        }

        /* 没有可显示帧就继续喂 AU，直到重排队列吐出一帧 */
        while (out < 0 && !pending_flush) {
            struct h264_slice_hdr hdr;
            int rc;

            if (sample_idx >= mp->demux.samples_count) {
                /* EOS：排空后回 sample 0 循环（素材以 IDR 开头） */
                sample_idx = 0;
                pending_flush = true;
                out = h264_dpb_next_output(&mp->dpb, true);
                break;
            }

            /*
             * mid-stream IDR 前先按档期排空上一 GOP 押着的帧：IDR 的
             * POC 复位为 0，一旦喂进去，min-POC bump 会先吐 IDR、旧帧
             * (POC 最大)反排其后 —— 屏上表现为每 GOP 边界一次帧序回跳
             * (slider 靶子第 7 趟必现，keyint=250)。sync 采样 = IDR。
             */
            if (sample_idx > 0 && mp->demux.samples[sample_idx].sync) {
                out = h264_dpb_next_output(&mp->dpb, true);
                if (out >= 0)
                    break; /* 本档期先出旧帧，sample 不前进 */
            }

            long long d0 = mp_get_now_us();
            rc = mp_decode_au(mp, sample_idx, &hdr);
            if (mp_get_now_us() - d0 > 50000)
                log_warn("slow decode_au %lldus @%u",
                         mp_get_now_us() - d0, sample_idx);
            if (rc < 0)
                goto decode_error;
            sample_idx++;
            if (rc > 0)
                continue;

            out = h264_dpb_next_output(&mp->dpb, false);
        }

        /* flush 刚排空：立即回到顶部从 sample 0 续喂，不空烧一个档期 */
        if (out < 0)
            continue;

        now = mp_get_now_us();
        if (now < next_frame_time)
            usleep(next_frame_time - now);
        else if (now > next_frame_time + 2 * 1000 * 1000) {
            log_warn("can't keep up, delay: %lld us", now - next_frame_time);
            next_frame_time = now;
        }

        mp_reclaim_free_items(mp);
        mp_enqueue_frame(mp, mp->fb_ids[out], out);
        next_frame_time += mp->frame_duration_us;
        // 节拍诊断：定速落后量(正=落后)。落后持续增长 = 吞吐不足
        if (++outputs % 300 == 0)
            log_info("mp pace: out=%u lag=%lldms", outputs,
                     (long long)(int64_t)(mp_get_now_us() - next_frame_time) / 1000);
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    log_info("==> mp_decode Thread Ended!");
    pthread_exit(NULL);
    return NULL;

decode_error:
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_DECODER_ERROR | MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    log_error("==> mp_decode Thread Ended (error)!");
    pthread_exit(NULL);
    return NULL;
}

static void mp_close_session(mediaplayer_t *mp)
{
    unsigned int i;

    if (!mp->session_open)
        return;

    for (i = 0; i < mp->vdec.cap_count; i++) {
        if (mp->fb_ids[i]) {
            drm_warpper_rm_fb(mp->drm_warpper, mp->fb_ids[i], mp->gem_handles[i]);
            mp->fb_ids[i] = 0;
            mp->gem_handles[i] = 0;
        }
    }
    vdec_close(&mp->vdec);
    mp4_close(&mp->demux);
    mp->session_open = false;
}

int mediaplayer_init(mediaplayer_t *mp, drm_warpper_t *drm_warpper)
{
    memset(mp, 0, sizeof(*mp));

    pthread_rwlock_init(&mp->thread.rwlock, NULL);
    atomic_store(&mp->running, 0);
    mp->drm_warpper = drm_warpper;

    log_info("==> mp Initalized!");
    return 0;
}

int mediaplayer_destroy(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }

    mediaplayer_stop(mp);
    pthread_rwlock_destroy(&mp->thread.rwlock);

    return 0;
}

int mediaplayer_remount_video_layer(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }
    int w = mp->frame_width  ? mp->frame_width  : VIDEO_WIDTH;
    int h = mp->frame_height ? mp->frame_height : VIDEO_HEIGHT;
    // 只是刷新几何记录，幂等；plane 实际状态由下一个 FLIP 决定
    return video_layer_ensure_mount(w, h);
}

/* play_video/start 的公共段：input_path 已就绪 */
static int mp_prepare_and_spawn(mediaplayer_t *mp)
{
    char video_path[32], media_path[32];
    const struct h264_sps *sps = NULL;
    unsigned int cap_count, max_ref, reorder, max_frame_num;
    unsigned int i;

    if (mp4_open(&mp->demux, mp->input_path) < 0) {
        log_error("mp4_open err: %s", mp->input_path);
        return -1;
    }
    mp->session_gen++;
    mp->session_open = true; /* demux 已开，之后统一走 close_session */

    if (mp->demux.codec != MP4_CODEC_H264) {
        log_error("not an H264 mp4");
        goto error;
    }
    if (mp->demux.max_sample_size > VDEC_OUTPUT_BUF_SIZE) {
        log_error("max sample %u exceeds output buffer", mp->demux.max_sample_size);
        goto error;
    }

    h264_parser_init(&mp->parser);
    if (h264_parser_parse_avcc(&mp->parser, mp->demux.extradata,
                               mp->demux.extradata_size) < 0) {
        log_error("avcC parse err");
        goto error;
    }
    for (i = 0; i < 32 && !sps; i++)
        sps = h264_parser_get_sps(&mp->parser, i);
    if (!sps) {
        log_error("no SPS in avcC");
        goto error;
    }
    if (!sps->frame_mbs_only_flag) {
        log_error("interlaced stream unsupported");
        goto error;
    }

    /* 编码尺寸(MB 对齐)与 CedarX 时代 parser 报告值一致(384x640/736x1280) */
    mp->frame_width = (sps->pic_width_in_mbs_minus1 + 1) * 16;
    mp->frame_height = (sps->pic_height_in_map_units_minus1 + 1) * 16;

    // 解码前先按尺寸把关并记录挂载几何(惰性：plane 由显示线程随首帧启用)。
    // 此时旧线程已 join、plane 已 disable，几何更新无并发帧提交
    if (!mp_size_supported(mp->frame_width, mp->frame_height)) {
        log_error("unsupported video size %dx%d", mp->frame_width, mp->frame_height);
        goto error;
    }
    video_layer_ensure_mount(mp->frame_width, mp->frame_height);

    mp->frame_duration_us = mp->demux.frame_duration_us ?
                            mp->demux.frame_duration_us : 33333;

    max_ref = sps->max_num_ref_frames ? sps->max_num_ref_frames : 1;
    max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
    if (sps->vui_reorder_valid) {
        /* refs 与重排共享 DPB。+4 = bump滞后1 + 入队未上屏1 + 在屏1 + 解码中1；
         * 阻塞 commit 返回=旧帧已离屏，在屏只押 1(NONBLOCK 在飞翻页时代要押
         * curr/pending 2 格即 +5，且真机实测再少 1 个会周期性等 25-50ms) */
        reorder = sps->vui_max_num_reorder_frames;
        cap_count = sps->vui_max_dec_frame_buffering + 4;
    } else {
        reorder = max_ref < VDEC_REORDER_DEPTH ? VDEC_REORDER_DEPTH : max_ref;
        cap_count = max_ref + reorder + 3;
    }
    {
        unsigned int cap_max =
            (unsigned int)mp->frame_width * mp->frame_height >=
                    VDEC_CAPTURE_LARGE_AREA ?
                VDEC_CAPTURE_BUF_MAX_LARGE : VDEC_CAPTURE_BUF_MAX_SMALL;
        if (cap_count < VDEC_CAPTURE_BUF_MIN)
            cap_count = VDEC_CAPTURE_BUF_MIN;
        if (cap_count > cap_max) {
            log_warn("capture need %u > budget %u (%dx%d), clamped;"
                     " 素材 ref/reorder 超预算可能饿槽",
                     cap_count, cap_max, mp->frame_width, mp->frame_height);
            cap_count = cap_max;
        }
    }

    if (vdec_find_device(video_path, sizeof(video_path),
                         media_path, sizeof(media_path)) < 0)
        goto error;

    if (vdec_open(&mp->vdec, video_path, media_path,
                  mp->frame_width, mp->frame_height,
                  cap_count, VDEC_OUTPUT_BUF_COUNT, VDEC_OUTPUT_BUF_SIZE) < 0)
        goto error;

    for (i = 0; i < mp->vdec.cap_count; i++) {
        if (drm_warpper_import_dmabuf_fb(mp->drm_warpper,
                                         mp->vdec.cap[i].dmabuf_fd,
                                         mp->vdec.cap_width,
                                         mp->vdec.cap_height,
                                         mp->vdec.cap_bytesperline,
                                         mp->vdec.cap_uv_offset,
                                         &mp->fb_ids[i],
                                         &mp->gem_handles[i]) < 0)
            goto error;
    }

    h264_dpb_init(&mp->dpb, cap_count, max_ref, max_frame_num, reorder);

    log_info("vdec: %ux%u max_ref=%u cap_bufs=%u dur=%uus",
             mp->frame_width, mp->frame_height, max_ref, cap_count,
             mp->frame_duration_us);

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

    // 等积压的 FLIP 回流，只剩屏上帧(curr)
    for (wait = 0; wait < 40 && mp->items_in_flight > 1; wait++) {
        usleep(10 * 1000);
        mp_reclaim_free_items(mp);
    }
    if (mp->items_in_flight > 2)
        log_warn("stop: %d frame items still in flight", mp->items_in_flight);

    // 关掉 video plane(最底层，露出 DEBE 黑背景 = 原黑帧效果)，
    // 此后 RmFB 碰不到在屏 fb，不会触发内核 atomic_remove_fb
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
