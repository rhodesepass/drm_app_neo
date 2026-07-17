/*
 * Mediaplayer：src/vdec 自制解码栈（MP4 demux + H264 parser/DPB + cedrus
 * V4L2 request API）→ dmabuf FB → drm_warpper atomic 翻页。
 *
 * 两个线程：解码线程全速解码(demux 是内存 sample 表、request 是同步等待，
 * 无再拆的收益)，帧进 smooth_q；pacer 线程按档期从 smooth_q 取出上屏。分开
 * 是为了让 VE spike 只堵解码侧——pacer 期间照吃储备帧出帧。smooth_q 满即
 * 反压解码线程，定速因此仍由 pacer 独家掌握。
 *
 * DPB 只由解码线程碰(标记/回收都在它那侧)，pacer 只搬 item，故无需加锁。
 *
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
#include "utils/spsc_queue.h"

#include "vdec/nalu.h"
#include "vdec/mp4_demux.h"
#include "vdec/h264_parser.h"
#include "vdec/h264_dpb.h"
#include "vdec/vdec_v4l2.h"

#define mp_get_now_us get_now_us

/* 设备后端的会话私有状态（mediaplayer_t.priv） */
typedef struct {
    struct mp4_demux   demux;
    struct h264_parser parser;
    struct h264_dpb    dpb;
    struct vdec_ctx    vdec;
    uint32_t           fb_ids[VDEC_MAX_CAP_BUFS];
    uint32_t           gem_handles[VDEC_MAX_CAP_BUFS];

    /* 解码线程 → pacer 的待上屏帧；容量 = smooth_bufs + 1(在手的那格) */
    spsc_bq_t          smooth_q;
    bool               smooth_q_ready;
    pthread_t          pacer_thread;
    bool               pacer_started;
    unsigned int       smooth_bufs;
} mp_dev_priv_t;

// main.c 提供，按解码尺寸记录 video 层挂载几何(720 档旧素材走 DEFE 放大)
extern int video_layer_ensure_mount(int src_w, int src_h);

static int mp_set_display_size(mediaplayer_t *mp, const struct h264_sps *sps)
{
    unsigned int crop_unit_x = 1;
    unsigned int crop_unit_y = 1;
    unsigned int crop_x, crop_y;

    mp->display_width = mp->frame_width;
    mp->display_height = mp->frame_height;
    if (!sps->frame_cropping_flag)
        return 0;

    /* H.264 7.4.2.1.1 crop units；隔行流已在调用前拒绝。 */
    if (!sps->separate_colour_plane_flag) {
        if (sps->chroma_format_idc == 1 || sps->chroma_format_idc == 2)
            crop_unit_x = 2;
        if (sps->chroma_format_idc == 1)
            crop_unit_y = 2;
    }

    crop_x = (sps->frame_crop_left_offset + sps->frame_crop_right_offset) *
             crop_unit_x;
    crop_y = (sps->frame_crop_top_offset + sps->frame_crop_bottom_offset) *
             crop_unit_y;
    if (crop_x >= (unsigned int)mp->frame_width ||
        crop_y >= (unsigned int)mp->frame_height) {
        log_error("invalid SPS crop %ux%u for coded size %dx%d",
                  crop_x, crop_y, mp->frame_width, mp->frame_height);
        return -1;
    }

    mp->display_width -= (int)crop_x;
    mp->display_height -= (int)crop_y;
    return 0;
}

static inline unsigned int mp_slow_threshold_us(const mediaplayer_t *mp)
{
    return mp->frame_duration_us + mp->frame_duration_us / 2;
}

/*
 * 平滑 buffer 档位：按 MemTotal 分。32M 机 CMA 预算已排满 → 0 格(退化成
 * "解出即等档期上屏"，与拆 pacer 前等价)；64M 机吃得下储备。见 config.h。
 * 读不到 meminfo 时按小内存兜底：多押 buffer 撑爆 CMA 比丢帧严重。
 */
static unsigned int mp_smooth_bufs(void)
{
    static int cached = -1;
    unsigned long total_kb = 0;
    char line[128];
    FILE *f;

    if (cached >= 0)
        return (unsigned int)cached;

    f = fopen(SYSINFO_MEMINFO_PATH, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "MemTotal: %lu kB", &total_kb) == 1)
                break;
        }
        fclose(f);
    }
    if (!total_kb) {
        log_warn("MemTotal unreadable, smooth bufs -> %d", MP_SMOOTH_BUFS_SMALL_MEM);
        cached = MP_SMOOTH_BUFS_SMALL_MEM;
        return (unsigned int)cached;
    }

    cached = total_kb >= MP_MEM_LARGE_THRESHOLD_KB ? MP_SMOOTH_BUFS_LARGE_MEM
                                                   : MP_SMOOTH_BUFS_SMALL_MEM;
    if (cached > MP_SMOOTH_BUFS_MAX)
        cached = MP_SMOOTH_BUFS_MAX;
    log_info("MemTotal %lukB -> smooth bufs %d", total_kb, cached);
    return (unsigned int)cached;
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
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    drm_warpper_queue_item_t *item;

    while (drm_warpper_try_dequeue_free_item(mp->drm_warpper,
                                             DRM_WARPPER_LAYER_VIDEO,
                                             &item) == 0) {
        uintptr_t ud = (uintptr_t)item->userdata;
        int slot = (int)(ud & 0xff) - 1;

        if (mp_trace_on())
            log_info("T R%d g%d", slot, (int)((ud >> 8) == mp->session_gen));
        if (slot >= 0 && (ud >> 8) == mp->session_gen)
            h264_dpb_set_on_screen(&p->dpb, slot, false);
        mp->items_in_flight--;
        free(item);
    }
}

/*
 * 睡到下一个档期，并把 *next 推到再下一个。*next==0 = 首帧：不睡，
 * 档期从当下起算。smooth_bufs=0 的解码线程与 pacer 线程共用。
 */
static void mp_pace_wait(mediaplayer_t *mp, long long *next)
{
    long long now = mp_get_now_us();

    if (!*next)
        *next = now;
    else if (now < *next)
        usleep(*next - now);
    else if (now > *next + 2 * 1000 * 1000) {
        log_warn("can't keep up, delay: %lld us", now - *next);
        *next = now;
    }
    *next += mp->frame_duration_us;
}

/*
 * 帧 item 工厂：占住 slot + in_flight 记账。只在解码线程调用(碰 dpb)。
 */
static drm_warpper_queue_item_t *mp_make_frame_item(mediaplayer_t *mp,
                                                    uint32_t fb_id, int slot)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    drm_warpper_queue_item_t *item = malloc(sizeof(*item));

    if (!item) {
        log_error("malloc err");
        return NULL;
    }
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_FLIP_FB;
    item->fb_id = fb_id;
    item->userdata = slot_to_userdata(mp, slot);
    item->on_heap = false; /* 帧类 item 由本模块经 free_queue 回收 */

    /* 交出去就算占住 slot：内容要保到上屏后离屏为止 */
    if (slot >= 0) {
        h264_dpb_set_on_screen(&p->dpb, slot, true);
        h264_dpb_mark_displayed(&p->dpb, slot);
    }
    if (mp_trace_on())
        log_info("T E%d", slot);
    mp->items_in_flight++;
    return item;
}

/*
 * pacer 线程(仅 smooth_bufs>0 时存在)：smooth_q → drm 显示队列，定速。
 *
 * 必须先睡档期再取帧：pacer 手上不留货，待发的 capture 格才恰好等于 ring
 * 深度，cap_count += smooth_bufs 的账才平。反过来(先取后睡)会白攥一帧过
 * 一个档期，等于凭空多吃一格。
 *
 * 不碰 dpb / items_in_flight，item 照旧经 free_queue 回解码线程回收。
 */
static void *mp_pacer_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    long long next_frame_time = 0;
    unsigned int outputs = 0;
    bool draining = false;

    log_info("==> mp_pacer Thread Started! dur=%uus smooth=%u",
             mp->frame_duration_us, p->smooth_bufs);

    while (1) {
        drm_warpper_queue_item_t *item;

        if (!draining)
            mp_pace_wait(mp, &next_frame_time);

        /* close 后仍会把队里剩的取完才 EPIPE，残帧不会漏成 in_flight */
        if (spsc_bq_pop(&p->smooth_q, (void **)&item) != 0)
            break;

        if (!draining) {
            /* 收摊：残帧不再定速，冲进显示队列让显示线程当跳帧回收，
             * stop 的 in_flight 等待才能及时收敛 */
            pthread_rwlock_rdlock(&mp->thread.rwlock);
            draining = mp->thread.requested_stop;
            pthread_rwlock_unlock(&mp->thread.rwlock);
        }

        drm_warpper_enqueue_display_item(mp->drm_warpper,
                                         DRM_WARPPER_LAYER_VIDEO, item);
        // 节拍诊断：定速落后量(正=落后) + ring 存量。存量长期见底 = VE 吞吐
        // 追不上素材帧率，储备只当了通道用，加深 buffer 也救不了(得降帧率)
        if (!draining && ++outputs % 300 == 0)
            log_info("mp pace: out=%u lag=%lldms ring=%u/%u", outputs,
                     (long long)(mp_get_now_us() - next_frame_time) / 1000,
                     (unsigned int)spsc_bq_count(&p->smooth_q), p->smooth_bufs);
    }

    log_info("==> mp_pacer Thread Ended!");
    return NULL;
}

/*
 * 解码一个 AU（单 slice）。返回:
 *  0 解码成功  1 非视频帧(跳过)  -1 错误(停播)
 */
static int mp_decode_au(mediaplayer_t *mp, unsigned int sample_idx,
                        struct h264_slice_hdr *hdr_out)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    const struct mp4_sample *sample = &p->demux.samples[sample_idx];
    const uint8_t *au = mp4_sample_data(&p->demux, sample_idx);
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
                                     p->demux.nal_length_size, &cursor, &n)) {
        unsigned int t = nalu_h264_type(&n);

        if (t == H264_NAL_SPS || t == H264_NAL_PPS) {
            h264_parser_parse_param_nal(&p->parser, &n);
        } else if (t == H264_NAL_SLICE || t == H264_NAL_IDR) {
            vcl_count++;
            if (have_hdr)
                continue;
            if (h264_parser_parse_slice(&p->parser, &n, hdr_out) < 0) {
                log_error("slice parse failed @%u", sample_idx);
                return -1;
            }
            h264_parser_compute_poc(&p->parser, hdr_out, &poc);
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

    if (h264_parser_fill_controls(&p->parser, hdr_out, &poc, &ctrl) < 0) {
        log_error("fill_controls failed @%u", sample_idx);
        return -1;
    }

    long long t0 = mp_get_now_us(), t1, t2;

    /* 无空槽 = 在飞帧太多，等显示线程回流(每 vblank 一次) */
    for (retry = 0; retry < 100; retry++) {
        slot = h264_dpb_begin_frame(&p->dpb, hdr_out, &poc, &ts, &ctrl);
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

    if (vdec_decode(&p->vdec, slot, ts, vcl.data, vcl.size, &ctrl) < 0) {
        log_error("decode failed @%u", sample_idx);
        h264_dpb_abort_frame(&p->dpb);
        return -1;
    }

    h264_dpb_end_frame(&p->dpb, hdr_out);
    t2 = mp_get_now_us();
    if (t2 - t0 > mp_slow_threshold_us(mp))
        log_warn("slow @%u: slot_wait=%lldus ve=%lldus size=%u",
                 sample_idx, t1 - t0, t2 - t1, vcl.size);
    return 0;
}

/*
 * 解码线程：出帧(POC 序)交给 pacer，限速靠 smooth_q 满时的反压。
 * smooth_bufs=0 时没有 pacer，自己睡档期后直接上屏(原路径)。
 */
static void *mp_decode_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    unsigned int sample_idx = 0;
    bool pending_flush = false;
    long long next_frame_time = 0;
    int out;

    log_info("==> mp_decode Thread Started! dur=%uus samples=%u",
             mp->frame_duration_us, p->demux.samples_count);

    while (1) {
        pthread_rwlock_rdlock(&mp->thread.rwlock);
        int requested_stop = mp->thread.requested_stop;
        pthread_rwlock_unlock(&mp->thread.rwlock);
        if (requested_stop)
            break;

        mp_reclaim_free_items(mp);

        /*
         * 先备货再等收货：本轮的"喂 AU 直到吐出一帧"跑在下面出帧的阻塞点
         * (睡档期 / ring 满)之前，GOP 边界的重排回填(IDR 后要连喂 reorder
         * 个 AU 才有输出)因此消化在本来就要空等的窗口里，不把 IDR 帧拖过
         * 档期 ~2 个 VE 周期(曾表现为每 GOP 一次 ~100ms 定格)。
         */

        /* GOP 边界(素材回绕)先按 flush 逐帧排空 DPB */
        if (pending_flush) {
            out = h264_dpb_next_output(&p->dpb, true);
            if (out < 0)
                pending_flush = false;
        } else {
            out = h264_dpb_next_output(&p->dpb, false);
        }

        /* 没有可显示帧就继续喂 AU，直到重排队列吐出一帧 */
        while (out < 0 && !pending_flush) {
            struct h264_slice_hdr hdr;
            int rc;

            if (sample_idx >= p->demux.samples_count) {
                /* EOS：排空后回 sample 0 循环（素材以 IDR 开头） */
                sample_idx = 0;
                pending_flush = true;
                out = h264_dpb_next_output(&p->dpb, true);
                break;
            }

            /*
             * mid-stream IDR 前先按档期排空上一 GOP 押着的帧：IDR 的
             * POC 复位为 0，一旦喂进去，min-POC bump 会先吐 IDR、旧帧
             * (POC 最大)反排其后 —— 屏上表现为每 GOP 边界一次帧序回跳
             * (slider 靶子第 7 趟必现，keyint=250)。sync 采样 = IDR。
             */
            if (sample_idx > 0 && p->demux.samples[sample_idx].sync) {
                out = h264_dpb_next_output(&p->dpb, true);
                if (out >= 0)
                    break; /* 本档期先出旧帧，sample 不前进 */
            }

            long long d0 = mp_get_now_us();
            rc = mp_decode_au(mp, sample_idx, &hdr);
            long long decode_us = mp_get_now_us() - d0;
            if (decode_us > mp_slow_threshold_us(mp))
                log_warn("slow decode_au %lldus @%u",
                         decode_us, sample_idx);
            if (rc < 0)
                goto decode_error;
            sample_idx++;
            if (rc > 0)
                continue;

            out = h264_dpb_next_output(&p->dpb, false);
        }

        /* flush 刚排空：立即回到顶部从 sample 0 续喂，不空烧一个档期 */
        if (out < 0)
            continue;

        drm_warpper_queue_item_t *item =
            mp_make_frame_item(mp, p->fb_ids[out], out);
        if (!item)
            goto decode_error;

        if (p->smooth_bufs) {
            /* 交 pacer 定速。ring 满则在此阻塞 = 解码限速阀；
             * 非 0 返回 = 队列已关，stop 中 */
            if (spsc_bq_push(&p->smooth_q, item) != 0) {
                mp->items_in_flight--;
                free(item);
                break;
            }
        } else {
            /* 无储备档：自己睡档期再上屏，与拆出 pacer 前逐字等价，
             * 一格 capture 都不多占 */
            mp_pace_wait(mp, &next_frame_time);
            mp_reclaim_free_items(mp);
            drm_warpper_enqueue_display_item(mp->drm_warpper,
                                             DRM_WARPPER_LAYER_VIDEO, item);
        }
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
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    unsigned int i;

    if (!mp->session_open)
        return;

    for (i = 0; i < p->vdec.cap_count; i++) {
        if (p->fb_ids[i]) {
            drm_warpper_rm_fb(mp->drm_warpper, p->fb_ids[i], p->gem_handles[i]);
            p->fb_ids[i] = 0;
            p->gem_handles[i] = 0;
        }
    }
    vdec_close(&p->vdec);
    mp4_close(&p->demux);
    if (p->smooth_q_ready) {
        spsc_bq_destroy(&p->smooth_q);
        p->smooth_q_ready = false;
    }
    mp->session_open = false;
}

int mediaplayer_init(mediaplayer_t *mp, drm_warpper_t *drm_warpper)
{
    memset(mp, 0, sizeof(*mp));

    mp->priv = calloc(1, sizeof(mp_dev_priv_t));
    if (!mp->priv) {
        log_error("mediaplayer priv alloc failed");
        return -1;
    }
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
    free(mp->priv);
    mp->priv = NULL;

    return 0;
}

int mediaplayer_remount_video_layer(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }
    int w = mp->display_width  ? mp->display_width  : VIDEO_WIDTH;
    int h = mp->display_height ? mp->display_height : VIDEO_HEIGHT;
    // 只是刷新几何记录，幂等；plane 实际状态由下一个 FLIP 决定
    return video_layer_ensure_mount(w, h);
}

/* play_video/start 的公共段：input_path 已就绪 */
static int mp_prepare_and_spawn(mediaplayer_t *mp)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    char video_path[32], media_path[32];
    const struct h264_sps *sps = NULL;
    unsigned int cap_count, max_ref, reorder, max_frame_num;
    unsigned int i;

    if (mp4_open(&p->demux, mp->input_path) < 0) {
        log_error("mp4_open err: %s", mp->input_path);
        return -1;
    }
    mp->session_gen++;
    mp->session_open = true; /* demux 已开，之后统一走 close_session */

    if (p->demux.codec != MP4_CODEC_H264) {
        log_error("not an H264 mp4");
        goto error;
    }
    if (p->demux.max_sample_size > VDEC_OUTPUT_BUF_SIZE) {
        log_error("max sample %u exceeds output buffer", p->demux.max_sample_size);
        goto error;
    }

    h264_parser_init(&p->parser);
    if (h264_parser_parse_avcc(&p->parser, p->demux.extradata,
                               p->demux.extradata_size) < 0) {
        log_error("avcC parse err");
        goto error;
    }
    for (i = 0; i < 32 && !sps; i++)
        sps = h264_parser_get_sps(&p->parser, i);
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
    if (mp_set_display_size(mp, sps) < 0)
        goto error;

    // 解码前记录挂载几何(惰性：plane 由显示线程随首帧启用)。
    // 此时旧线程已 join、plane 已 disable，几何更新无并发帧提交
    if (video_layer_ensure_mount(mp->display_width, mp->display_height) < 0)
        goto error;

    mp->frame_duration_us = p->demux.frame_duration_us ?
                            p->demux.frame_duration_us : 33333;

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
        /* 平滑储备是解码正确性预算之外的额外格，钳制之后再叠 —— 否则大内存
         * 机型的储备会被 720 档的 cap_max 吃掉，等于白配 */
        p->smooth_bufs = mp_smooth_bufs();
        if (cap_count + p->smooth_bufs > VDEC_MAX_CAP_BUFS)
            p->smooth_bufs = VDEC_MAX_CAP_BUFS - cap_count;
        cap_count += p->smooth_bufs;
    }

    /* ring 深度 == smooth_bufs：待发格数 = ring + 解码线程手里那格，比原路径
     * (只有手里那格)恰好多 smooth_bufs 格，与上面 cap_count 的加法对上 */
    if (p->smooth_bufs) {
        if (spsc_bq_init(&p->smooth_q, p->smooth_bufs) != 0) {
            log_error("smooth queue init err");
            goto error;
        }
        p->smooth_q_ready = true;
    }

    if (vdec_find_device(video_path, sizeof(video_path),
                         media_path, sizeof(media_path)) < 0)
        goto error;

    if (vdec_open(&p->vdec, video_path, media_path,
                  mp->frame_width, mp->frame_height,
                  cap_count, VDEC_OUTPUT_BUF_COUNT, VDEC_OUTPUT_BUF_SIZE) < 0)
        goto error;
    p->vdec.slow_threshold_us = mp_slow_threshold_us(mp);

    for (i = 0; i < p->vdec.cap_count; i++) {
        if (drm_warpper_import_dmabuf_fb(mp->drm_warpper,
                                         p->vdec.cap[i].dmabuf_fd,
                                         p->vdec.cap_width,
                                         p->vdec.cap_height,
                                         p->vdec.cap_bytesperline,
                                         p->vdec.cap_uv_offset,
                                         &p->fb_ids[i],
                                         &p->gem_handles[i]) < 0)
            goto error;
    }

    h264_dpb_init(&p->dpb, cap_count, max_ref, max_frame_num, reorder);

    log_info("vdec: coded=%ux%u display=%dx%d max_ref=%u cap_bufs=%u(smooth %u) dur=%uus",
             mp->frame_width, mp->frame_height,
             mp->display_width, mp->display_height, max_ref, cap_count,
             p->smooth_bufs, mp->frame_duration_us);

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    atomic_store(&mp->running, 1);

    /* pacer 先起：它阻塞等首帧，解码线程一出帧就有人接 */
    if (p->smooth_bufs) {
        if (pthread_create(&p->pacer_thread, NULL, mp_pacer_thread, mp) != 0) {
            log_error("pacer thread create err");
            atomic_store(&mp->running, 0);
            goto error;
        }
        p->pacer_started = true;
    }

    if (pthread_create(&mp->decode_thread, NULL, mp_decode_thread, mp) != 0) {
        log_error("decode thread create err");
        atomic_store(&mp->running, 0);
        goto error;
    }

    return 0;

error:
    /* pacer 可能已阻塞在 pop：先关队列放它走，再拆会话 */
    if (p->pacer_started) {
        spsc_bq_close(&p->smooth_q);
        pthread_join(p->pacer_thread, NULL);
        p->pacer_started = false;
    }
    p->smooth_bufs = 0;
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
    mp_dev_priv_t *p;
    int wait;

    if (!mp) {
        return -1;
    }

    if (!atomic_load(&mp->running)) {
        return 0;
    }
    p = (mp_dev_priv_t *)mp->priv;

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.requested_stop = 1;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    /* 解码线程可能正阻塞在 smooth_q push 上，光置 requested_stop 叫不醒它；
     * 关队列同时放走两边。pacer 会把队里残帧取完(不再定速)才收工，
     * 这些 item 照常经 free_queue 回流，下面的 in_flight 等待才收敛 */
    if (p->smooth_q_ready)
        spsc_bq_close(&p->smooth_q);

    pthread_join(mp->decode_thread, NULL);
    if (p->pacer_started) {
        pthread_join(p->pacer_thread, NULL);
        p->pacer_started = false;
    }
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
