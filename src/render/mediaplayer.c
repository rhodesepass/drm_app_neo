#include "mediaplayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include "drm_warpper.h"
#include "log.h"
#include "cdx_config.h"
#include "CdxParser.h"
#include "vdecoder.h"
#include "memoryAdapter.h"
#include "config.h"
#include "srgn_drm.h"

/* external cedarx plugin entry */
extern void AddVDPlugin(void);


/* get current time in us */
long long mp_get_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

/* parser thread: read bitstream and feed decoder */
static void *mp_parser_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    CdxParserT *parser = mp->parser;
    VideoDecoder *decoder = mp->decoder;
    CdxPacketT packet;
    VideoStreamDataInfo dataInfo;
    int ret;
    int validSize;
    int requestSize;
    int streamNum;
    int trytime = 0;
    unsigned char *buf = NULL;

    buf = malloc(1024 * 1024);
    if (buf == NULL) {
        log_error("parser thread malloc err");
        goto parser_exit;
    }

    memset(&packet, 0, sizeof(packet));
    memset(&dataInfo, 0, sizeof(dataInfo));

    log_info("parser thread start");

    startagain:
    while (0 == CdxParserPrefetch(parser, &packet)) {
        usleep(50);

        pthread_rwlock_rdlock(&mp->thread.rwlock);
        int state = mp->thread.state;
        int requested_stop = mp->thread.requested_stop;
        pthread_rwlock_unlock(&mp->thread.rwlock);

        if (requested_stop || (state & (MEDIAPLAYER_PARSER_ERROR |
                                        MEDIAPLAYER_DECODER_ERROR |
                                        MEDIAPLAYER_DECODE_FINISH))) {
            log_info("parser:get exit flag");
            break;
        }

        validSize = VideoStreamBufferSize(decoder, 0) - VideoStreamDataSize(decoder, 0);
        requestSize = packet.length;

        if (trytime >= 2000) {
            log_error("try time too much");
            pthread_rwlock_wrlock(&mp->thread.rwlock);
            mp->thread.state |= MEDIAPLAYER_PARSER_ERROR;
            pthread_rwlock_unlock(&mp->thread.rwlock);
            break;
        }

        if (packet.type == CDX_MEDIA_VIDEO && ((packet.flags & MINOR_STREAM) == 0)) {
            if (requestSize > validSize) {
                usleep(50 * 1000);
                trytime++;
                continue;
            }

            ret = RequestVideoStreamBuffer(decoder, requestSize,
                                           (char **)&packet.buf, &packet.buflen,
                                           (char **)&packet.ringBuf, &packet.ringBufLen, 0);
            if (ret != 0) {
                log_error("RequestVideoStreamBuffer err, request=%d, valid=%d",
                          requestSize, validSize);
                usleep(50 * 1000);
                continue;
            }

            if (packet.buflen + packet.ringBufLen < requestSize) {
                log_error("RequestVideoStreamBuffer err, not enough space");
                pthread_rwlock_wrlock(&mp->thread.rwlock);
                mp->thread.state |= MEDIAPLAYER_PARSER_ERROR;
                pthread_rwlock_unlock(&mp->thread.rwlock);
                break;
            }
        } else {
            packet.buf = buf;
            packet.buflen = packet.length;
            CdxParserRead(parser, &packet);
            continue;
        }

        trytime = 0;
        streamNum = VideoStreamFrameNum(decoder, 0);
        if (streamNum > 1024) {
            usleep(50 * 1000);
        }

        ret = CdxParserRead(parser, &packet);
        if (ret != 0) {
            log_error("cdxparser read err");
            pthread_rwlock_wrlock(&mp->thread.rwlock);
            mp->thread.state |= MEDIAPLAYER_PARSER_ERROR;
            pthread_rwlock_unlock(&mp->thread.rwlock);
            break;
        }

        memset(&dataInfo, 0, sizeof(dataInfo));
        dataInfo.pData        = packet.buf;
        dataInfo.nLength      = packet.length;
        dataInfo.nPts         = packet.pts;
        dataInfo.nPcr         = packet.pcr;
        dataInfo.bIsFirstPart = !!(packet.flags & FIRST_PART);
        dataInfo.bIsLastPart  = !!(packet.flags & LAST_PART);

        ret = SubmitVideoStreamData(decoder, &dataInfo, 0);
        if (ret != 0) {
            log_error("SubmitVideoStreamData err");
            pthread_rwlock_wrlock(&mp->thread.rwlock);
            mp->thread.state |= MEDIAPLAYER_PARSER_ERROR;
            pthread_rwlock_unlock(&mp->thread.rwlock);
            break;
        }
    }
    
    if(CdxParserGetStatus(parser) == PSR_EOS){
        log_info("eos, start again!");
        CdxParserSeekTo(parser, 0, AW_SEEK_CLOSEST);  
        goto startagain;
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.end_of_stream = 1;
    mp->thread.state |= MEDIAPLAYER_PARSER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);

parser_exit:
    if (buf) {
        free(buf);
    }
    log_info("parser thread exit");
    pthread_exit(NULL);
    return NULL;
}

/* decoder thread: decode and copy one frame to output buffer */
static void *mp_decoder_thread(void *param)
{
    mediaplayer_t *mp = (mediaplayer_t *)param;
    VideoDecoder *decoder = mp->decoder;
    int ret;
    int end_of_stream = 0;
    long long next_frame_time = 0;


    next_frame_time = mp_get_now_us() + 1000000 * 1000 / mp->framerate;

    log_info("decoder thread start, target fps: %d", mp->framerate);

    while (1) {
        long long current_time = mp_get_now_us();
        if (current_time < next_frame_time) {
            usleep(next_frame_time - current_time);
        }
        usleep(50);

        if (current_time > next_frame_time + 2 * 1000 * 1000) {
            log_warn("can't keep up, delay: %lld us", current_time - next_frame_time);
            next_frame_time = current_time + 1000000 * 1000 / mp->framerate;
        }

        pthread_rwlock_rdlock(&mp->thread.rwlock);
        end_of_stream = mp->thread.end_of_stream;
        int state = mp->thread.state;
        int requested_stop = mp->thread.requested_stop;
        pthread_rwlock_unlock(&mp->thread.rwlock);

        if (requested_stop) {
            log_info("req stop,exiting");
            break;
        }

        if (state & (MEDIAPLAYER_PARSER_ERROR | MEDIAPLAYER_DECODER_ERROR)) {
            log_error("err state,exiting");
            break;
        }

        // first try to dequeue free buffer from drm_warpper
        drm_warpper_queue_item_t* item;
        while(drm_warpper_try_dequeue_free_item(mp->drm_warpper, DRM_WARPPER_LAYER_VIDEO, &item) == 0){
            VideoPicture* pic = (VideoPicture*)item->userdata;
            if(pic){
                ReturnPicture(decoder, pic);
            }
            free(item);
        }

        // long long start = mp_get_now_us();
        ret = DecodeVideoStream(decoder, end_of_stream, 0, 0, 0);
        // long long finish = mp_get_now_us();
        // log_debug("frame time: %lld us", finish - start);

        if (end_of_stream == 1 && ret == VDECODE_RESULT_NO_BITSTREAM) {
            log_info("data end!!!");
            break;
        }

        if (ret == VDECODE_RESULT_KEYFRAME_DECODED ||
            ret == VDECODE_RESULT_FRAME_DECODED) {
            int validNum = ValidPictureNum(decoder, 0);
            if (validNum > 0) {
                VideoPicture *picture = RequestPicture(decoder, 0);
                if (!picture) {
                    log_error("RequestPicture err");
                    continue;
                }

                if (picture->nWidth != MEDIAPLAYER_FRAME_WIDTH ||
                    picture->nHeight != MEDIAPLAYER_FRAME_HEIGHT) {
                    log_error("err size, expect %dx%d, actual %dx%d",
                              MEDIAPLAYER_FRAME_WIDTH, MEDIAPLAYER_FRAME_HEIGHT,
                              picture->nWidth, picture->nHeight);
                    ReturnPicture(decoder, picture);
                    pthread_rwlock_wrlock(&mp->thread.rwlock);
                    mp->thread.state |= MEDIAPLAYER_DECODER_ERROR;
                    pthread_rwlock_unlock(&mp->thread.rwlock);
                    break;
                }

                // int dataLen = picture->nWidth * picture->nHeight * 3 / 2;
                // memcpy(mp->output_buf, picture->pData0,
                //        picture->nWidth * picture->nHeight);
                // memcpy(mp->output_buf + picture->nWidth * picture->nHeight,
                //        picture->pData1,
                //        picture->nWidth * picture->nHeight / 2);

                // ReturnPicture(decoder, picture);

                // 把拿到的picture直接交给drm ioctl挂上去(Zero Copy!)
                drm_warpper_queue_item_t* item_to_display = malloc(sizeof(drm_warpper_queue_item_t));
                if(item_to_display == NULL){
                    log_error("malloc err");
                    ReturnPicture(decoder, picture);
                    continue;
                }

                item_to_display->mount.type = DRM_SRGN_MOUNT_FB_TYPE_YUV;
                item_to_display->mount.ch0_addr = (uint32_t)picture->pData0;
                item_to_display->mount.ch1_addr = (uint32_t)picture->pData1;
                item_to_display->mount.ch2_addr = 0;
                item_to_display->userdata = (void*)picture;

                drm_warpper_enqueue_display_item(mp->drm_warpper, DRM_WARPPER_LAYER_VIDEO, item_to_display);

                next_frame_time = next_frame_time + 1000000 * 1000 / mp->framerate;
            }
        }

        if (ret < 0) {
            log_error("DecodeVideoStream err: %d", ret);
            pthread_rwlock_wrlock(&mp->thread.rwlock);
            mp->thread.state |= MEDIAPLAYER_DECODER_ERROR;
            pthread_rwlock_unlock(&mp->thread.rwlock);
            break;
        }
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.state |= MEDIAPLAYER_DECODER_EXIT;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    log_info("decoder thread exit");
    pthread_exit(NULL);
    return NULL;
}

/* internal helper: release parser/decoder/memops */
static void mp_cleanup_internal(mediaplayer_t *mp)
{
    if (mp->parser) {
        CdxParserClose(mp->parser);
        mp->parser = NULL;
    }
    if (mp->decoder) {
        DestroyVideoDecoder(mp->decoder);
        mp->decoder = NULL;
    }
    if (mp->memops) {
        CdcMemClose(mp->memops);
        mp->memops = NULL;
    }
}

int mediaplayer_init(mediaplayer_t *mp,drm_warpper_t *drm_warpper)
{

    memset(mp, 0, sizeof(*mp));

    pthread_mutex_init(&mp->parser_mutex, NULL);
    pthread_rwlock_init(&mp->thread.rwlock, NULL);
    mp->thread.end_of_stream = 0;
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    mp->running = 0;
    memset(mp->video_path, 0, sizeof(mp->video_path));

    mp->drm_warpper = drm_warpper;

    AddVDPlugin();

    return 0;
}

int mediaplayer_destroy(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }

    /* ensure stopped */
    mediaplayer_stop(mp);

    mp_cleanup_internal(mp);

    pthread_rwlock_destroy(&mp->thread.rwlock);
    pthread_mutex_destroy(&mp->parser_mutex);

    return 0;
}

int mediaplayer_play_video(mediaplayer_t *mp, const char *file)
{
    if (!mp || !file) {
        log_error("invalid params");
        return -1;
    }

    if (mp->running) {
        log_error("mediaplayer is running");
        return -1;
    }

    memset(mp->input_uri, 0, sizeof(mp->input_uri));
    snprintf(mp->input_uri, sizeof(mp->input_uri), "file://%s", file);

    mp->memops = MemAdapterGetOpsS();
    if (!mp->memops) {
        log_error("MemAdapterGetOpsS err");
        return -1;
    }
    CdcMemOpen(mp->memops);

    memset(&mp->source, 0, sizeof(CdxDataSourceT));
    memset(&mp->media_info, 0, sizeof(CdxMediaInfoT));

    mp->source.uri = mp->input_uri;

    int force_exit = 0;
    CdxStreamT *stream = NULL;
    int ret = CdxParserPrepare(&mp->source, 0, &mp->parser_mutex,
                               &force_exit, &mp->parser, &stream, NULL, NULL);
    if (ret < 0 || !mp->parser) {
        log_error("CdxParserPrepare err");
        mp_cleanup_internal(mp);
        return -1;
    }

    ret = CdxParserGetMediaInfo(mp->parser, &mp->media_info);
    if (ret != 0) {
        log_error("CdxParserGetMediaInfo err");
        mp_cleanup_internal(mp);
        return -1;
    }

    

    mp->decoder = CreateVideoDecoder();
    if (!mp->decoder) {
        log_error("CreateVideoDecoder err");
        mp_cleanup_internal(mp);
        return -1;
    }

    VConfig vConfig;
    VideoStreamInfo vInfo;
    memset(&vConfig, 0, sizeof(VConfig));
    memset(&vInfo, 0, sizeof(VideoStreamInfo));

    struct CdxProgramS *program =
        &mp->media_info.program[mp->media_info.programIndex];

    /* only use first video stream */
    vInfo.eCodecFormat          = program->video[0].eCodecFormat;
    vInfo.nWidth                = program->video[0].nWidth;
    vInfo.nHeight               = program->video[0].nHeight;
    vInfo.nFrameRate            = program->video[0].nFrameRate;
    vInfo.nFrameDuration        = program->video[0].nFrameDuration;
    vInfo.nAspectRatio          = program->video[0].nAspectRatio;
    vInfo.bIs3DStream           = program->video[0].bIs3DStream;
    vInfo.nCodecSpecificDataLen = program->video[0].nCodecSpecificDataLen;
    vInfo.pCodecSpecificData    = program->video[0].pCodecSpecificData;

    mp->framerate = vInfo.nFrameRate;

    vConfig.eOutputPixelFormat  = PIXEL_FORMAT_YUV_MB32_420;
    vConfig.nDeInterlaceHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4di_num", BUF_CNT_4_DI);
    vConfig.nDisplayHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4list_num", BUF_CNT_4_LIST);
    vConfig.nRotateHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4rotate_num", BUF_CNT_4_ROTATE);
    vConfig.nDecodeSmoothFrameBufferNum =
        GetConfigParamterInt("pic_4smooth_num", BUF_CNT_4_SMOOTH);
    vConfig.memops = mp->memops;
    vConfig.nVbvBufferSize = VBVBUFFERSIZE;

    ret = InitializeVideoDecoder(mp->decoder, &vInfo, &vConfig);
    if (ret != 0) {
        log_error("InitializeVideoDecoder err");
        mp_cleanup_internal(mp);
        return -1;
    }

    /* reset thread flags */
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.end_of_stream = 0;
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    mp->running = 1;

    if (pthread_create(&mp->parser_thread, NULL, mp_parser_thread, mp) != 0) {
        log_error("parser create err");
        mp->running = 0;
        mp_cleanup_internal(mp);
        return -1;
    }

    if (pthread_create(&mp->decoder_thread, NULL, mp_decoder_thread, mp) != 0) {
        log_error("decoder create err");
        pthread_rwlock_wrlock(&mp->thread.rwlock);
        mp->thread.requested_stop = 1;
        pthread_rwlock_unlock(&mp->thread.rwlock);
        pthread_join(mp->parser_thread, NULL);
        mp->running = 0;
        mp_cleanup_internal(mp);
        return -1;
    }

    // /* wait for both threads to finish */
    // pthread_join(mp->parser_thread, NULL);
    // pthread_join(mp->decoder_thread, NULL);
    // mp->running = 0;

    // /* check final state */
    // pthread_rwlock_rdlock(&mp->thread.rwlock);
    // int final_state = mp->thread.state;
    // pthread_rwlock_unlock(&mp->thread.rwlock);

    // mp_cleanup_internal(mp);

    // if (final_state & (MEDIAPLAYER_PARSER_ERROR | MEDIAPLAYER_DECODER_ERROR)) {
    //     log_error("play failed, err state");
    //     return -1;
    // }
    // if (!(final_state & MEDIAPLAYER_DECODE_FINISH)) {
    //     log_error("decode failed, no frame");
    //     return -1;
    // }

    return 0;
}

int mediaplayer_stop(mediaplayer_t *mp)
{
    if (!mp) {
        return -1;
    }

    if (!mp->running) {
        return 0;
    }

    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.requested_stop = 1;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    pthread_join(mp->parser_thread, NULL);
    pthread_join(mp->decoder_thread, NULL);
    mp->running = 0;

    mp_cleanup_internal(mp);

    return 0;
}

int mediaplayer_set_video(mediaplayer_t *mp, const char *path)
{
    if (!mp || !path) {
        log_error("invalid params");
        return -1;
    }

    if (mp->running) {
        log_error("cannot set video while playing, stop first");
        return -1;
    }

    memset(mp->video_path, 0, sizeof(mp->video_path));
    snprintf(mp->video_path, sizeof(mp->video_path), "%s", path);
    log_info("video path set to: %s", mp->video_path);

    return 0;
}

int mediaplayer_start(mediaplayer_t *mp)
{
    if (!mp) {
        log_error("invalid paramst");
        return -1;
    }

    if (mp->running) {
        log_warn("mediaplayer already running");
        return 0;
    }

    if (strlen(mp->video_path) == 0) {
        log_error("no video path set");
        return -1;
    }

    memset(mp->input_uri, 0, sizeof(mp->input_uri));
    snprintf(mp->input_uri, sizeof(mp->input_uri), "file://%s", mp->video_path);

    mp->memops = MemAdapterGetOpsS();
    if (!mp->memops) {
        log_error("MemAdapterGetOpsS err");
        return -1;
    }
    CdcMemOpen(mp->memops);

    memset(&mp->source, 0, sizeof(CdxDataSourceT));
    memset(&mp->media_info, 0, sizeof(CdxMediaInfoT));

    mp->source.uri = mp->input_uri;

    int force_exit = 0;
    CdxStreamT *stream = NULL;
    int ret = CdxParserPrepare(&mp->source, 0, &mp->parser_mutex,
                               &force_exit, &mp->parser, &stream, NULL, NULL);
    if (ret < 0 || !mp->parser) {
        log_error("CdxParserPrepare err");
        mp_cleanup_internal(mp);
        return -1;
    }

    ret = CdxParserGetMediaInfo(mp->parser, &mp->media_info);
    if (ret != 0) {
        log_error("CdxParserGetMediaInfo err");
        mp_cleanup_internal(mp);
        return -1;
    }

    mp->decoder = CreateVideoDecoder();
    if (!mp->decoder) {
        log_error("CreateVideoDecoder err");
        mp_cleanup_internal(mp);
        return -1;
    }

    VConfig vConfig;
    VideoStreamInfo vInfo;
    memset(&vConfig, 0, sizeof(VConfig));
    memset(&vInfo, 0, sizeof(VideoStreamInfo));

    struct CdxProgramS *program =
        &mp->media_info.program[mp->media_info.programIndex];

    /* only use first video stream */
    vInfo.eCodecFormat          = program->video[0].eCodecFormat;
    vInfo.nWidth                = program->video[0].nWidth;
    vInfo.nHeight               = program->video[0].nHeight;
    vInfo.nFrameRate            = program->video[0].nFrameRate;
    vInfo.nFrameDuration        = program->video[0].nFrameDuration;
    vInfo.nAspectRatio          = program->video[0].nAspectRatio;
    vInfo.bIs3DStream           = program->video[0].bIs3DStream;
    vInfo.nCodecSpecificDataLen = program->video[0].nCodecSpecificDataLen;
    vInfo.pCodecSpecificData    = program->video[0].pCodecSpecificData;

    mp->framerate = vInfo.nFrameRate;

    vConfig.eOutputPixelFormat  = PIXEL_FORMAT_YUV_MB32_420;
    vConfig.nDeInterlaceHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4di_num", BUF_CNT_4_DI);
    vConfig.nDisplayHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4list_num", BUF_CNT_4_LIST);
    vConfig.nRotateHoldingFrameBufferNum =
        GetConfigParamterInt("pic_4rotate_num", BUF_CNT_4_ROTATE);
    vConfig.nDecodeSmoothFrameBufferNum =
        GetConfigParamterInt("pic_4smooth_num", BUF_CNT_4_SMOOTH);
    vConfig.memops = mp->memops;
    vConfig.nVbvBufferSize = VBVBUFFERSIZE;

    ret = InitializeVideoDecoder(mp->decoder, &vInfo, &vConfig);
    if (ret != 0) {
        log_error("InitializeVideoDecoder err");
        mp_cleanup_internal(mp);
        return -1;
    }

    /* reset thread flags */
    pthread_rwlock_wrlock(&mp->thread.rwlock);
    mp->thread.end_of_stream = 0;
    mp->thread.state = 0;
    mp->thread.requested_stop = 0;
    pthread_rwlock_unlock(&mp->thread.rwlock);

    mp->running = 1;

    if (pthread_create(&mp->parser_thread, NULL, mp_parser_thread, mp) != 0) {
        log_error("parser create err");
        mp->running = 0;
        mp_cleanup_internal(mp);
        return -1;
    }

    if (pthread_create(&mp->decoder_thread, NULL, mp_decoder_thread, mp) != 0) {
        log_error("decoder create err");
        pthread_rwlock_wrlock(&mp->thread.rwlock);
        mp->thread.requested_stop = 1;
        pthread_rwlock_unlock(&mp->thread.rwlock);
        pthread_join(mp->parser_thread, NULL);
        mp->running = 0;
        mp_cleanup_internal(mp);
        return -1;
    }

    log_info("playback started");
    return 0;
}

mp_status_t mediaplayer_get_status(mediaplayer_t *mp)
{
    if (!mp) {
        return MP_STATUS_ERROR;
    }

    if (!mp->running) {
        return MP_STATUS_STOPPED;
    }

    return MP_STATUS_PLAYING;
}
