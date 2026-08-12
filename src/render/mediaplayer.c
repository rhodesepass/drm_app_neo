/*
 * Mediaplayer：srgnvdec stream → dmabuf FB → drm_warpper atomic 翻页。
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

#include <errno.h>
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

#include <srgnvdec/vdec.h>
#include <srgnvdec/stream.h>
/*
 * 链错配置的库时控制结构体布局对不上，编译器不会吭声，真机上才炸 —— CMake
 * 已在 configure 期校验过一道，这里再挡住绕过 CMake 的手写构建。
 */
#if EPASS_PLATFORM_F1C != SRGNVDEC_UAPI_PRESTABLE
#error "srgnvdec 的 uapi 配置与 EPASS_PLATFORM 不符 (f1c=prestable, d1s=stable)"
#endif

#define mp_get_now_us get_now_us

/* 设备后端的会话私有状态（mediaplayer_t.priv） */
typedef struct {
    struct srgnvdec_stream *stream;
    struct srgnvdec_stream_info info;
    uint32_t           fb_ids[VDEC_PP_MAX_FRAMES];
    uint32_t           gem_handles[VDEC_PP_MAX_FRAMES];

    /*
     * 显示变换(倒装机型 = vflip,见 docs/boe-flip-180.md)。非 identity 时库内
     * 走 PP 管线,fb_ids 索引的是 PP 输出池;identity 时即解码 cap 池——对本
     * 文件而言只是 vdec_out 枚举/借帧 API 的活动池换了,代码不分叉。
     */
    struct vdec_transform tform;
    /* 借出去未归还的帧,按活动池 slot 索引(item 回流时凭 slot 归还) */
    struct vdec_frame  *inflight[VDEC_PP_MAX_FRAMES];

    /* 解码线程 → pacer 的待上屏帧；容量 = smooth_bufs + 1(在手的那格) */
    spsc_bq_t          smooth_q;
    bool               smooth_q_ready;
    pthread_t          pacer_thread;
    bool               pacer_started;
    unsigned int       smooth_bufs;
} mp_dev_priv_t;

// main.c 提供，按解码尺寸记录 video 层挂载几何(720 档旧素材走 DEFE 放大)
extern int video_layer_ensure_mount(int src_w, int src_h);

static inline unsigned int mp_slow_threshold_us(const mediaplayer_t *mp)
{
    return mp->frame_duration_us * 2;
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

/*
 * 收 free_queue：归还离屏帧并释放 item。slot 索引活动池,凭 inflight[] 找回
 * 借出的帧交还库(库内按帧来源解 DPB 显示保持或还 PP 池)。
 */
static void mp_reclaim_free_items(mediaplayer_t *mp)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    drm_warpper_queue_item_t *item;

    while (drm_warpper_try_dequeue_free_item(mp->drm_warpper,
                                             DRM_WARPPER_LAYER_VIDEO,
                                             &item) == 0) {
        uintptr_t ud = (uintptr_t)item->userdata;
        int slot = (int)(ud & 0xff) - 1;
        bool mine = (ud >> 8) == mp->session_gen;

        if (mp_trace_on())
            log_info("T R%d g%d", slot, (int)mine);
        if (slot >= 0 && mine && p->inflight[slot]) {
            srgnvdec_stream_frame_release(p->stream, p->inflight[slot]);
            p->inflight[slot] = NULL;
        }
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
 * 帧 item 工厂：登记借来的帧 + in_flight 记账。只在解码线程调用。
 * 帧的 DPB 显示保持已由 vdec_frame_acquire 处理,这里只做 item 侧账。
 */
static drm_warpper_queue_item_t *mp_make_frame_item(mediaplayer_t *mp,
                                                    struct vdec_frame *f)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    drm_warpper_queue_item_t *item = malloc(sizeof(*item));

    if (!item) {
        log_error("malloc err");
        return NULL;
    }
    memset(item, 0, sizeof(*item));
    item->type = DRM_WARPPER_ITEM_FLIP_FB;
    item->fb_id = p->fb_ids[f->slot];
    item->userdata = slot_to_userdata(mp, f->slot);
    item->on_heap = false; /* 帧类 item 由本模块经 free_queue 回收 */

    p->inflight[f->slot] = f;
    if (mp_trace_on())
        log_info("T E%d", f->slot);
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
#ifdef MP_TIMING_DEBUG
        // 节拍诊断：定速落后量(正=落后) + ring 存量。存量长期见底 = VE 吞吐
        // 追不上素材帧率，储备只当了通道用，加深 buffer 也救不了(得降帧率)
        if (!draining && ++outputs % 300 == 0)
            log_info("mp pace: out=%u lag=%lldms ring=%u/%u", outputs,
                     (long long)(mp_get_now_us() - next_frame_time) / 1000,
                     (unsigned int)spsc_bq_count(&p->smooth_q), p->smooth_bufs);
#else
        (void)outputs;
#endif
    }

    log_info("==> mp_pacer Thread Ended!");
    return NULL;
}

static int mp_wait_capture(void *arg)
{
    mediaplayer_t *mp = arg;

    usleep(5 * 1000);
    mp_reclaim_free_items(mp);
    pthread_rwlock_rdlock(&mp->thread.rwlock);
    int requested_stop = mp->thread.requested_stop;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    return requested_stop ? -1 : 0;
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
             mp->frame_duration_us, p->info.sample_count);

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
            out = srgnvdec_stream_next_output(p->stream, true);
            if (out < 0)
                pending_flush = false;
        } else {
            out = srgnvdec_stream_next_output(p->stream, false);
        }

        /* 没有可显示帧就继续喂 AU，直到重排队列吐出一帧 */
        while (out < 0 && !pending_flush) {
            int rc;

            if (sample_idx >= p->info.sample_count) {
                /* EOS：排空后回 sample 0 循环（素材以 IDR 开头） */
                sample_idx = 0;
                pending_flush = true;
                out = srgnvdec_stream_next_output(p->stream, true);
                break;
            }

            /*
             * mid-stream IDR 前先按档期排空上一 GOP 押着的帧：IDR 的
             * POC 复位为 0，一旦喂进去，min-POC bump 会先吐 IDR、旧帧
             * (POC 最大)反排其后 —— 屏上表现为每 GOP 边界一次帧序回跳
             * (slider 靶子第 7 趟必现，keyint=250)。sync 采样 = IDR,
             * H.265 侧 = IRAP,语义相同。
             */
            if (sample_idx > 0 &&
                srgnvdec_stream_sample_is_sync(p->stream, sample_idx)) {
                out = srgnvdec_stream_next_output(p->stream, true);
                if (out >= 0)
                    break; /* 本档期先出旧帧，sample 不前进 */
            }

#ifdef MP_TIMING_DEBUG
            long long d0 = mp_get_now_us();
#endif
            rc = srgnvdec_stream_decode_sample(p->stream, sample_idx,
                                                mp_wait_capture, mp);
#ifdef MP_TIMING_DEBUG
            long long decode_us = mp_get_now_us() - d0;
            if (decode_us > mp_slow_threshold_us(mp))
                log_warn("slow decode_au %lldus @%u",
                         decode_us, sample_idx);
#endif
            if (rc == SRGNVDEC_STREAM_SOURCE_LOST)
                goto source_lost;
            if (rc == SRGNVDEC_STREAM_AGAIN)
                break;
            if (rc < 0)
                goto decode_error;
            sample_idx++;
            if (rc > 0)
                continue;

            out = srgnvdec_stream_next_output(p->stream, false);
        }

        /* flush 刚排空：立即回到顶部从 sample 0 续喂，不空烧一个档期 */
        if (out < 0)
            continue;

        drm_warpper_queue_item_t *item;
        struct vdec_frame *frame = NULL;

        /* 借帧;PP 池竭时等显示线程回流(每 vblank 一次),同旧 flip 池等法 */
        {
            int retry;

            for (retry = 0; retry < 100; retry++) {
                frame = srgnvdec_stream_frame_acquire(p->stream, out);
                if (frame || errno != EAGAIN)
                    break;
                usleep(5 * 1000);
                mp_reclaim_free_items(mp);
            }
        }
        if (!frame) {
            log_error("frame acquire failed @slot %d: %s", out,
                      strerror(errno));
            goto decode_error;
        }

        item = mp_make_frame_item(mp, frame);
        if (!item) {
            srgnvdec_stream_frame_release(p->stream, frame);
            goto decode_error;
        }

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

source_lost:
    /* 片源(通常 SD 上的视频)读取失败：干净停解码，额外置 SOURCE_LOST 供上层区分 */
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_SOURCE_LOST | MEDIAPLAYER_DECODER_ERROR | MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    log_error("==> mp_decode Thread Ended (source lost)!");
    pthread_exit(NULL);
    return NULL;
}

static void mp_close_session(mediaplayer_t *mp)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    unsigned int i;

    if (!mp->session_open)
        return;

    /* fb_ids 索引活动池;数量必须在 stream_close(会 detach PP)之前取 */
    {
        unsigned int fb_n = srgnvdec_stream_output_count(p->stream);

        for (i = 0; i < fb_n; i++) {
            if (p->fb_ids[i]) {
                drm_warpper_rm_fb(mp->drm_warpper, p->fb_ids[i],
                                  p->gem_handles[i]);
                p->fb_ids[i] = 0;
                p->gem_handles[i] = 0;
            }
        }
    }
    memset(p->inflight, 0, sizeof(p->inflight));
    srgnvdec_stream_close(p->stream);
    p->stream = NULL;
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

/*
 * 整机是否倒装(视频层内容要不要 PP 补 Y 翻转)。存在 DT key 即倒装——运行时
 * 检测,D1s 上这个 key 不存在,天然为假;MP_FORCE_YFLIP=0/1 可覆盖(调试/在
 * 平装机上验证 PP 链)。检测一次即缓存。
 */
static bool mp_detect_yflip(void)
{
    static int cached = -1;
    const char *env;

    if (cached >= 0)
        return cached;

    env = getenv("MP_FORCE_YFLIP");
    if (env)
        cached = (env[0] == '1');
    else
        cached = (access(MP_YFLIP_DT_PATH, F_OK) == 0);

    log_info("video yflip(scanout) = %d", cached);
    return cached;
}

/* play_video/start 的公共段：input_path 已就绪 */
static int mp_prepare_and_spawn(mediaplayer_t *mp)
{
    mp_dev_priv_t *p = (mp_dev_priv_t *)mp->priv;
    const struct srgnvdec_stream_info *info;
    struct srgnvdec_stream_config stream_config;
    unsigned int cap_count;
    unsigned int out_size, cap_floor, pp_out_count;
    unsigned int i;

    /* 头文件宏与库实际编的 uapi 对账；错配 = 控制集布局错位，启动即拦 */
    if (!srgnvdec_abi_ok()) {
        log_error("srgnvdec ABI mismatch: lib uapi != compile-time macros");
        return -1;
    }

    /* 先清上一会话的错误位:失败路径直接 return -1 不会走到下面,残留的
     * DECODER_ERROR 会被 prts 轮询当成本次的异步失败重复上报。此刻旧解码
     * 线程已在 mediaplayer_stop() 里 join 完,无并发写。 */
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    if (srgnvdec_stream_open(&p->stream, mp->input_path, &info) < 0) {
        log_error("stream open err: %s", mp->input_path);
        return -1;
    }
    mp->session_gen++;
    mp->session_open = true;
    p->info = *info;
    mp->frame_width = (int)info->coded_width;
    mp->frame_height = (int)info->coded_height;
    mp->display_width = (int)info->display_width;
    mp->display_height = (int)info->display_height;
    mp->frame_duration_us = info->frame_duration_us;
    cap_count = info->capture_buffers;
    cap_floor = info->capture_floor;
    out_size = info->codec == SRGNVDEC_STREAM_CODEC_H265 ?
               VDEC_OUTPUT_BUF_SIZE_H265 : VDEC_OUTPUT_BUF_SIZE;

    /* 目前 app 只有倒装补偿一种变换;attach 时由库校验布局可行性 */
    memset(&p->tform, 0, sizeof(p->tform));
    p->tform.vflip = mp_detect_yflip();

    // 解码前记录挂载几何(惰性：plane 由显示线程随首帧启用)。
    // 此时旧线程已 join、plane 已 disable，几何更新无并发帧提交
    if (video_layer_ensure_mount(mp->display_width, mp->display_height) < 0)
        goto error;

    {
        unsigned int cap_max =
            (unsigned int)mp->frame_width * mp->frame_height >=
                    VDEC_CAPTURE_LARGE_AREA ?
                VDEC_CAPTURE_BUF_MAX_LARGE : VDEC_CAPTURE_BUF_MAX_SMALL;
        if (cap_count > cap_max) {
            log_warn("capture need %u > budget %u (%dx%d), clamped;"
                     " 素材 ref/reorder 超预算可能饿槽",
                     cap_count, cap_max, mp->frame_width, mp->frame_height);
            cap_count = cap_max;
        }
        /* floor 压过预算：钳到参考集放不下解码根本进行不下去(见上) */
        if (cap_count < cap_floor)
            cap_count = cap_floor;
        /* 平滑储备是解码正确性预算之外的额外格，钳制之后再叠 —— 否则大内存
         * 机型的储备会被 720 档的 cap_max 吃掉，等于白配 */
        p->smooth_bufs = mp_smooth_bufs();
        if (cap_count + p->smooth_bufs > VDEC_PP_MAX_FRAMES)
            p->smooth_bufs = VDEC_PP_MAX_FRAMES - cap_count;
        cap_count += p->smooth_bufs;
    }

    /*
     * 带变换时:显示保持从解码 cap 转移到 PP 输出池。cap_count 里的显示保持
     * (在屏1 + 入队未上屏1 = MP_PP_HOLD)连同平滑储备一并对冲掉——这些帧现在
     * 活在 PP 池,解码 cap 只需留到 DPB 参考逻辑放行,净 CMA ≈ 不变。
     */
    pp_out_count = MP_PP_HOLD + p->smooth_bufs;
    if (!vdec_transform_is_identity(&p->tform)) {
        unsigned int reclaim = pp_out_count;

        if (cap_count > reclaim + cap_floor)
            cap_count -= reclaim;
        else
            cap_count = cap_floor;
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

    memset(&stream_config, 0, sizeof(stream_config));
    stream_config.capture_buffers = cap_count;
    stream_config.output_buffers = VDEC_OUTPUT_BUF_COUNT;
    stream_config.output_buffer_size = out_size;
    stream_config.slow_threshold_us = mp_slow_threshold_us(mp);
    if (srgnvdec_stream_start(p->stream, &stream_config) < 0)
        goto error;

    if (srgnvdec_stream_attach_pp(p->stream, &p->tform, pp_out_count) < 0) {
        log_error("pp attach failed (transform rot%u%s%s)", p->tform.rot,
                  p->tform.hflip ? "+hflip" : "",
                  p->tform.vflip ? "+vflip" : "");
        goto error;
    }

    /* FB 建在活动池上(PP 开 = 旋转输出池,关 = 解码 cap 池),代码不分叉 */
    {
        struct vdec_out_geom g;

        srgnvdec_stream_output_geom(p->stream, &g);
        if (g.rect_x || g.rect_y) {
            /* drm_warpper 的视频层不带 src 偏移;真旋转需求出现时再扩 */
            log_error("pp rect offset %u,%u unsupported by display path",
                      g.rect_x, g.rect_y);
            goto error;
        }
        for (i = 0; i < srgnvdec_stream_output_count(p->stream); i++) {
            if (drm_warpper_import_dmabuf_fb(mp->drm_warpper,
                                             srgnvdec_stream_output_dmabuf(
                                                 p->stream, i),
                                             g.width, g.height,
                                             g.bytesperline, g.uv_offset,
                                             &p->fb_ids[i],
                                             &p->gem_handles[i]) < 0)
                goto error;
        }
    }

    log_info("vdec: codec=%s coded=%ux%u display=%dx%d max_ref=%u"
             " cap_bufs=%u(smooth %u) pp=%d out_bufs=%u dur=%uus",
             p->info.codec == SRGNVDEC_STREAM_CODEC_H264 ? "h264" : "h265",
             mp->frame_width, mp->frame_height,
             mp->display_width, mp->display_height,
             p->info.max_ref_frames, cap_count, p->smooth_bufs,
             (int)srgnvdec_stream_pp_enabled(p->stream),
             srgnvdec_stream_output_count(p->stream), mp->frame_duration_us);

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

bool mediaplayer_source_lost(mediaplayer_t *mp)
{
    bool lost;
    if (!mp)
        return false;
    pthread_rwlock_rdlock(&mp->thread.rwlock);
    lost = (mp->thread.state & MEDIAPLAYER_SOURCE_LOST) != 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    return lost;
}

bool mediaplayer_decode_error(mediaplayer_t *mp)
{
    int state;
    if (!mp)
        return false;
    pthread_rwlock_rdlock(&mp->thread.rwlock);
    state = mp->thread.state;
    pthread_rwlock_unlock(&mp->thread.rwlock);
    return (state & MEDIAPLAYER_DECODER_ERROR) &&
           !(state & MEDIAPLAYER_SOURCE_LOST);
}
