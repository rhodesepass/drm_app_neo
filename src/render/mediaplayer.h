#pragma once

#include "cdx_config.h"
#include "CdxParser.h"
#include "vdecoder.h"
#include "memoryAdapter.h"
#include <stdint.h>
#include <pthread.h>
#include "config.h"
#include "drm_warpper.h"

/* fixed output frame size */
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
    int                  running;
    int                  framerate;
    
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
