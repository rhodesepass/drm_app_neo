#include <render/fbdraw.h>
#include <stdint.h>

#include "utils/log.h"
#include "config.h"
#include "overlay/overlay.h"
#include "overlay/transitions.h"
#include "driver/drm_warpper.h"
#include "utils/timer.h"
#include "render/layer_animation.h"
#include "utils/imgscale.h"

#define STB_IMAGE_IMPLEMENTATION
#include "utils/stb_image.h"

#if OVERLAY_USE_C8
// 层不可见(离屏/alpha 0/已清成透明)时把本次颜色池写进动态段并上传。
// 三个 transition 入口都在 overlay_abort 之后被调,旧内容已离屏,立即换表安全
static void oltr_apply_palette(oltr_params_t* params){
    c8pal_restore_baked();
    c8pal_write_range(C8PAL_DYN_BASE, params->c8_pool, params->c8_pool_n);
    c8pal_commit();
}
#endif

static void oltr_callback_cleanup(void* userdata,bool is_last){
    oltr_callback_t* callback = (oltr_callback_t*)userdata;
    log_trace("oltr_callback_cleanup");
    if(callback->on_heap){
        free(callback);
    }
    log_trace("oltr_callback_cleanup: done");
}

// 渐变过渡，准备完成后无耗时操作，不需要使用worker。
void overlay_transition_fade(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params){
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    overlay->request_abort = 0;

    // alpha 置 0 后直绘单 buffer，绘制过程不可见
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

#if OVERLAY_USE_C8
    oltr_apply_palette(params);
#endif

    uint32_t* vaddr = (uint32_t*)overlay->overlay_buf.vaddr;

    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;
    fbdst.fmt = FBDRAW_OVERLAY_FMT;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;
    fbdraw_fill_rect(&fbdst, &dst_rect, params->background_color);

    if(params->image_addr){
        fbsrc.vaddr = params->image_addr;
        fbsrc.width = params->image_w;
        fbsrc.height = params->image_h;
        fbsrc.fmt = FBDRAW_FMT_ARGB8888;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->image_w;
        src_rect.h = params->image_h;

        dst_rect.x = OVERLAY_WIDTH / 2  - params->image_w / 2;
        dst_rect.y = OVERLAY_HEIGHT / 2 - params->image_h / 2;
        dst_rect.w = params->image_w;
        dst_rect.h = params->image_h;
        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }

    // 渐变到图层
    layer_animation_fade_in(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        0
    );


    // 渐变到透明
    layer_animation_fade_out(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        2 * params->duration
    );

    prts_timer_handle_t middle_cb_handler;
    prts_timer_create(&middle_cb_handler,params->duration,0,1,callback->middle_cb,callback->middle_cb_userdata);
    
    prts_timer_handle_t end_cb_handler;
    prts_timer_create(&end_cb_handler,3 * params->duration,0,1,callback->end_cb,callback->end_cb_userdata);
    
    prts_timer_handle_t callback_cleanup_handler;
    prts_timer_create(&callback_cleanup_handler,3 * params->duration + 500 * 1000,0,1,oltr_callback_cleanup,callback);
}

// 贝塞尔函数移动过渡。 不需要使用worker
void overlay_transition_move(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params){
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    overlay->request_abort = 0;

    // 先挪到屏外再直绘单 buffer，绘制过程不可见
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, SCREEN_WIDTH, 0);
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);

#if OVERLAY_USE_C8
    oltr_apply_palette(params);
#endif

    uint32_t* vaddr = (uint32_t*)overlay->overlay_buf.vaddr;

    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;
    fbdst.fmt = FBDRAW_OVERLAY_FMT;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;
    fbdraw_fill_rect(&fbdst, &dst_rect, params->background_color);

    if(params->image_addr){
        fbsrc.vaddr = params->image_addr;
        fbsrc.width = params->image_w;
        fbsrc.height = params->image_h;
        fbsrc.fmt = FBDRAW_FMT_ARGB8888;
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->image_w;
        src_rect.h = params->image_h;
        dst_rect.x = OVERLAY_WIDTH / 2  - params->image_w / 2;
        dst_rect.y = OVERLAY_HEIGHT / 2 - params->image_h / 2;
        dst_rect.w = params->image_w;
        dst_rect.h = params->image_h;
        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }

    layer_animation_ease_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        SCREEN_WIDTH, 0,
        0, 0,
        params->duration, 
        0
    );

    layer_animation_ease_in_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, 0,
        -SCREEN_WIDTH, 0,
        params->duration, 
        2 * params->duration
    );

    prts_timer_handle_t middle_cb_handler;
    prts_timer_create(&middle_cb_handler,params->duration,0,1,callback->middle_cb,callback->middle_cb_userdata);
    
    prts_timer_handle_t end_cb_handler;
    prts_timer_create(&end_cb_handler,3 * params->duration,0,1,callback->end_cb,callback->end_cb_userdata);

    prts_timer_handle_t callback_cleanup_handler;
    prts_timer_create(&callback_cleanup_handler,3 * params->duration + 500 * 1000,0,1,oltr_callback_cleanup,callback);

}

typedef struct {
    overlay_t* overlay;
    oltr_params_t* params;

    int curr_frame;
    int total_frames;
    int frames_per_stage;

    int image_start_x;
    int image_end_x;

    int image_start_y;


    int* bezeir_values;

    bool middle_cb_called;
    oltr_callback_t* callback;

} swipe_worker_data_t;

typedef enum{
    SWIPE_DRAW_CONTENT,
    SWIPE_DRAW_IDLE,
    SWIPE_DRAW_CLEAR
} swipe_draw_state_t;


static void swipe_cleanup(swipe_worker_data_t* data){
    prts_timer_cancel(data->overlay->overlay_timer_handle);
    free(data->bezeir_values);
    data->bezeir_values = NULL;
    data->overlay->overlay_timer_handle = 0;
    if(data->callback->on_heap){
        free(data->callback);
    }
    return;
}

// 每一帧绘制的时候来调用。 来自 overlay_worker 线程
// 双缓冲的建议按照状态机+绘制的方法来写。
// 先计算这次要画哪些东西，然后绘制，最后交换buffer。
static void swipe_worker(void *userdata,int skipped_frames){
    swipe_worker_data_t* data = (swipe_worker_data_t*)userdata;
    // log_trace("swipe_worker: skipped_frames=%d,curr_frame=%d,total_frames=%d", skipped_frames, data->curr_frame, data->total_frames);
    
    // 是否要求我们退出
    if(data->overlay->request_abort){
        swipe_cleanup(data);
        log_debug("swipe worker: request abort");
        return;
    }
    
    uint32_t* vaddr = (uint32_t*)data->overlay->overlay_buf.vaddr;


    // 状态转移 START
    swipe_draw_state_t draw_state;
    int draw_start_x;
    int draw_end_x;


    if (data->curr_frame < data->frames_per_stage){
        draw_state = SWIPE_DRAW_CONTENT;

        // 单 buffer 内容累积，每帧只需推进一帧
        draw_start_x = data->bezeir_values[data->curr_frame];
        if(data->curr_frame == 0){
            draw_start_x = 0;
        }
        if(data->curr_frame + 1 < data->frames_per_stage){
            draw_end_x = data->bezeir_values[data->curr_frame + 1];
        }
        else{
            draw_end_x = UI_WIDTH;
        }
    }
    else if (data->curr_frame < 2 * data->frames_per_stage){
        draw_state = SWIPE_DRAW_IDLE;
        // stub to make compiler happy.
        draw_start_x = 0;
        draw_end_x = 0;
        if(!data->middle_cb_called){
            if(data->callback->middle_cb){
                data->callback->middle_cb(data->callback->middle_cb_userdata, false);
            }
            data->middle_cb_called = true;
        }
    }
    else{
        draw_state = SWIPE_DRAW_CLEAR;
        int stage_frame = data->curr_frame - 2 * data->frames_per_stage;
        draw_start_x = data->bezeir_values[stage_frame];
        if(stage_frame == 0){
            draw_start_x = 0;
        }
        if(stage_frame + 1 < data->frames_per_stage){
            draw_end_x = data->bezeir_values[stage_frame + 1];
        }
        else{
            draw_end_x = UI_WIDTH;
        }
    }


    int draw_image_start_x;
    int draw_image_end_x;
    int draw_image_w;

    if(draw_state == SWIPE_DRAW_CONTENT){
        if(draw_start_x < data->image_start_x){
            draw_image_start_x = data->image_start_x;
        }
        else{
            draw_image_start_x = draw_start_x;
        }
        if(draw_end_x > data->image_end_x){
            draw_image_end_x = data->image_end_x;
        }
        else{
            draw_image_end_x = draw_end_x;
        }

        draw_image_w = draw_image_end_x - draw_image_start_x;
        if(draw_image_w < 0){
            draw_image_w = 0;
        }
    }
    // 状态转移 END

    // 绘制 START
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;
    fbdst.fmt = FBDRAW_OVERLAY_FMT;

    if(draw_state == SWIPE_DRAW_CONTENT){
        // 填充颜色
        dst_rect.x = draw_start_x;
        dst_rect.y = 0;
        dst_rect.w = draw_end_x - draw_start_x;
        dst_rect.h = OVERLAY_HEIGHT;
        fbdraw_fill_rect(&fbdst, &dst_rect, data->params->background_color);

        // 绘制图片
        fbsrc.vaddr = data->params->image_addr;
        fbsrc.width = data->params->image_w;
        fbsrc.height = data->params->image_h;
        fbsrc.fmt = FBDRAW_FMT_ARGB8888;

        src_rect.x = draw_image_start_x - data->image_start_x;
        src_rect.y = 0;
        src_rect.w = draw_image_w;
        src_rect.h = data->params->image_h;

        dst_rect.x = draw_image_start_x;
        dst_rect.y = data->image_start_y;
        dst_rect.w = draw_image_w;
        dst_rect.h = OVERLAY_HEIGHT;
        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }
    else if(draw_state == SWIPE_DRAW_CLEAR){
        // 填充颜色
        dst_rect.x = draw_start_x;
        dst_rect.y = 0;
        dst_rect.w = draw_end_x - draw_start_x;
        dst_rect.h = OVERLAY_HEIGHT;
        fbdraw_fill_rect(&fbdst, &dst_rect, 0x00000000);
    }

    data->curr_frame ++;
    if(data->curr_frame >= data->total_frames){
        // 单 buffer：结束后把图层泊回屏外，让后续过渡的直绘阶段不可见
        drm_warpper_set_layer_coord(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);
        if(data->callback->end_cb){
            data->callback->end_cb(data->callback->end_cb_userdata, true);
        }
        swipe_cleanup(data);
        return;
    }

}

// 定时器回调。来自普瑞塞斯 的 rt 启动的 sigev_thread 线程。
static void swipe_timer_cb(void *userdata,bool is_last){
    swipe_worker_data_t* data = (swipe_worker_data_t*)userdata;
    overlay_worker_schedule(data->overlay,swipe_worker,data);
}

// 类似drm_app的过渡效果，但是使用贝塞尔，需要使用worker。
void overlay_transition_swipe(overlay_t* overlay,oltr_callback_t* callback,oltr_params_t* params){

#if OVERLAY_USE_C8
    // idx 0 在任何表里都是全透明,先换表再清 buffer 顺序无所谓
    oltr_apply_palette(params);
#endif

    // 先清空 buffer 再上坐标，摆到屏内时内容已是全透明
    memset(overlay->overlay_buf.vaddr, 0, OVERLAY_BUF_BYTES);

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

    static swipe_worker_data_t swipe_worker_data;
    swipe_worker_data.overlay = overlay;
    swipe_worker_data.curr_frame = 0;
    swipe_worker_data.frames_per_stage = params->duration / OVERLAY_ANIMATION_STEP_TIME;
    swipe_worker_data.total_frames = 3 * params->duration / OVERLAY_ANIMATION_STEP_TIME;
    swipe_worker_data.middle_cb_called = false;
    swipe_worker_data.callback = callback;
    swipe_worker_data.params = params;
    swipe_worker_data.image_start_x = UI_WIDTH / 2 - params->image_w / 2;
    swipe_worker_data.image_end_x = UI_WIDTH / 2 + params->image_w / 2;
    swipe_worker_data.image_start_y = UI_HEIGHT / 2 - params->image_h / 2;

    swipe_worker_data.bezeir_values = malloc(swipe_worker_data.frames_per_stage * sizeof(int));
    if(swipe_worker_data.bezeir_values == NULL){
        log_error("malloc failed??");
        return;
    }

    int32_t ctlx1 = LV_BEZIER_VAL_FLOAT(0.42);
    int32_t ctly1 = LV_BEZIER_VAL_FLOAT(0);
    int32_t ctx2 = LV_BEZIER_VAL_FLOAT(0.58);
    int32_t cty2 = LV_BEZIER_VAL_FLOAT(1);


    for(int i = 0; i < swipe_worker_data.frames_per_stage; i++){
        uint32_t t = lv_map(i, 0, swipe_worker_data.frames_per_stage, 0, LV_BEZIER_VAL_MAX);
        int32_t step = lv_cubic_bezier(t, ctlx1, ctly1, ctx2, cty2);
        int32_t new_value;
        new_value = step * UI_WIDTH;
        new_value = new_value >> LV_BEZIER_VAL_SHIFT;
        swipe_worker_data.bezeir_values[i] = new_value;
    }

    overlay->request_abort = 0;

    // 我们在这里设置永远触发，其实是在worker里面注销定时器。
    // 我们要保证，就算有跳帧发生，最后一次触发的事件也能传到我们的回调里面
    // 在那里处理资源回收的问题。

    // 不在定时器回调用is_last处理资源回收的原因是，worker和定时器回调是两个线程
    // 如果worker在运行的时候你free了它的内存，就会直接UAF。
    prts_timer_create(
        &overlay->overlay_timer_handle,
        params->duration,
        OVERLAY_ANIMATION_STEP_TIME,
        -1,
        swipe_timer_cb,
        &swipe_worker_data
    );

}

void overlay_transition_load_image(oltr_params_t* params){
    params->c8_pool_n = 0;
    if(params->type == TRANSITION_TYPE_NONE){
        return;
    }
    load_img_assets(params->image_path, &params->image_addr, &params->image_w, &params->image_h);
    if(params->image_addr){
        imgscale_rescale_nn_rgba(&params->image_addr, &params->image_w, &params->image_h, params->src_upscale, params->src_downscale);
    }
#if OVERLAY_USE_C8
    // 只量化不上传(旧 overlay 可能还在退场),上传在各 transition 入口
    uint32_t bg = params->background_color | 0xFF000000;
    c8pal_pool_add(params->c8_pool, &params->c8_pool_n,
                   (int)(sizeof(params->c8_pool) / sizeof(params->c8_pool[0])), &bg, 1);
    if(params->image_addr){
        uint32_t tmp[C8PAL_QUOTA_TRIMG];
        int n = c8pal_load_or_quantize(params->image_path, params->image_addr,
                                       params->image_w, params->image_h,
                                       C8PAL_QUOTA_TRIMG, tmp);
        if(n > 0)
            c8pal_pool_add(params->c8_pool, &params->c8_pool_n,
                           (int)(sizeof(params->c8_pool) / sizeof(params->c8_pool[0])), tmp, n);
    }
#endif
    log_debug("(transition) loaded image: %s, w: %d, h: %d", params->image_path, params->image_w, params->image_h);
}

void overlay_transition_free_image(oltr_params_t* params){
    if(params->image_addr){
        free(params->image_addr);
        params->image_addr = NULL;
        log_debug("(transition) freed image: %s", params->image_path);
    }
}