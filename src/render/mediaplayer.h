#pragma once

#include "config.h"

#if HAVE_CEDARX

#include "cdx_config.h"
#include <CdxParser.h>
#include <vdecoder.h>
#include <memoryAdapter.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include "driver/drm_warpper.h"

/* native output frame size; 720 档另接受 VIDEO_LEGACY_* 旧素材(DEFE 放大) */
#define MEDIAPLAYER_FRAME_WIDTH   VIDEO_WIDTH
#define MEDIAPLAYER_FRAME_HEIGHT  VIDEO_HEIGHT

/* internal state flags */
#define MEDIAPLAYER_PARSER_ERROR   (1 << 0)
#define MEDIAPLAYER_DECODER_ERROR  (1 << 1)
#define MEDIAPLAYER_DECODE_FINISH  (1 << 2)
#define MEDIAPLAYER_PARSER_EXIT    (1 << 3)
#define MEDIAPLAYER_DECODER_EXIT   (1 << 4)

typedef struct MultiThreadCtx {
    pthread_rwlock_t rwlock;
    int end_of_stream;
    int state;
    int requested_stop;
} MultiThreadCtx;

typedef struct {
    VideoDecoder        *decoder;
    CdxParserT          *parser;
    CdxDataSourceT       source;
    CdxMediaInfoT        media_info;
    struct ScMemOpsS    *memops;

    pthread_t            parser_thread;
    pthread_t            decoder_thread;
    pthread_mutex_t      parser_mutex;
    MultiThreadCtx       thread;

    char                 input_uri[256];
    char                 video_path[256];
    atomic_int           running;
    int                  framerate;

    /* 当前视频的解码帧尺寸(parser 报告)，决定 video 层挂载方式 */
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

/* decode one frame from file into buf (YUV MB32 420, fixed size) */
int mediaplayer_play_video(mediaplayer_t *mediaplayer, const char *file);

/* stop current decoding if running */
int mediaplayer_stop(mediaplayer_t *mediaplayer);

/* set video file path (takes effect on next play) */
int mediaplayer_set_video(mediaplayer_t *mediaplayer, const char *path);

/* start playback (non-blocking) */
int mediaplayer_start(mediaplayer_t *mediaplayer);

/* get current status: "stopped", "playing", */
mp_status_t mediaplayer_get_status(mediaplayer_t *mediaplayer);

/* 按当前视频尺寸强制重挂 video 层(未播放过则挂 native 黑帧 buffer)。
   供过渡 middle_cb 在画面被遮盖的时机调用 */
int mediaplayer_remount_video_layer(mediaplayer_t *mediaplayer);

#else /* !HAVE_CEDARX */

/* Cedarx not available - provide stub definitions */
#include "driver/drm_warpper.h"

typedef struct {
    int dummy;
    drm_warpper_t *drm_warpper;
} mediaplayer_t;

typedef enum {
    MP_STATUS_PLAYING,
    MP_STATUS_STOPPED,
    MP_STATUS_ERROR,
} mp_status_t;

/* Stub functions when Cedarx is not available */
static inline int mediaplayer_init(mediaplayer_t *mp, drm_warpper_t *drm) {
    mp->drm_warpper = drm;
    return 0;
}
static inline int mediaplayer_destroy(mediaplayer_t *mp) { (void)mp; return 0; }
static inline int mediaplayer_stop(mediaplayer_t *mp) { (void)mp; return 0; }
static inline int mediaplayer_set_video(mediaplayer_t *mp, const char *p) { (void)mp; (void)p; return -1; }
static inline int mediaplayer_start(mediaplayer_t *mp) { (void)mp; return -1; }
static inline int mediaplayer_play_video(mediaplayer_t *mp, const char *f) { (void)mp; (void)f; return -1; }
static inline mp_status_t mediaplayer_get_status(mediaplayer_t *mp) { (void)mp; return MP_STATUS_STOPPED; }
static inline int mediaplayer_remount_video_layer(mediaplayer_t *mp) { (void)mp; return 0; }

#endif /* HAVE_CEDARX */
