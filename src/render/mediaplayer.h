#pragma once

#include "config.h"

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

#include "driver/drm_warpper.h"

/* native output frame size; 720 档另接受 VIDEO_LEGACY_* 旧素材(DEFE 放大) */
#define MEDIAPLAYER_FRAME_WIDTH   VIDEO_WIDTH
#define MEDIAPLAYER_FRAME_HEIGHT  VIDEO_HEIGHT

/* internal state flags */
#define MEDIAPLAYER_DECODER_ERROR  (1 << 1)
#define MEDIAPLAYER_DECODER_EXIT   (1 << 4)

typedef struct MultiThreadCtx {
    pthread_rwlock_t rwlock;
    int state;
    int requested_stop;
} MultiThreadCtx;

typedef struct {
    /* 解码后端私有会话状态：设备 = 自制 demux/parser/DPB + cedrus V4L2，
     * PC = ffmpeg。init 时由后端分配，destroy 时释放；公共字段留在本结构，
     * ipc_handler 等直接读 video_path/running。 */
    void                *priv;
    bool                 session_open;

    pthread_t            decode_thread;
    MultiThreadCtx       thread;

    char                 input_path[256];
    char                 video_path[256];
    atomic_int           running;
    uint32_t             frame_duration_us;

    /* 未从 free_queue 回流的帧 item 数；stop 时据此等待离屏 */
    int                  items_in_flight;
    /* 会话代号，进 item userdata 高位；跨会话回流的 item 不碰新 DPB */
    uint32_t             session_gen;

    /* 当前视频的编码帧尺寸(SPS 报告，MB 对齐)，决定 video 层挂载方式 */
    int                  frame_width;
    int                  frame_height;

    drm_warpper_t       *drm_warpper;
} mediaplayer_t;

typedef enum {
    MP_STATUS_PLAYING,
    MP_STATUS_STOPPED,
    MP_STATUS_ERROR,
} mp_status_t;

/* initialize mediaplayer context */
int mediaplayer_init(mediaplayer_t *mediaplayer, drm_warpper_t *drm_warpper);

/* destroy mediaplayer context and release all resources */
int mediaplayer_destroy(mediaplayer_t *mediaplayer);

/* start looping playback of an mp4 file (non-blocking) */
int mediaplayer_play_video(mediaplayer_t *mediaplayer, const char *file);

/* stop current decoding if running */
int mediaplayer_stop(mediaplayer_t *mediaplayer);

/* set video file path (takes effect on next play) */
int mediaplayer_set_video(mediaplayer_t *mediaplayer, const char *path);

/* start playback (non-blocking) */
int mediaplayer_start(mediaplayer_t *mediaplayer);

/* get current status: "stopped", "playing", */
mp_status_t mediaplayer_get_status(mediaplayer_t *mediaplayer);

/* 按当前视频尺寸刷新 video 层几何记录(幂等，plane 实际状态由下一个 FLIP 决定)。
   供过渡 middle_cb 在画面被遮盖的时机调用 */
int mediaplayer_remount_video_layer(mediaplayer_t *mediaplayer);
