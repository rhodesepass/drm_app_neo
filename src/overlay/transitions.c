#include <render/fbdraw.h>
#include <stdint.h>

#include "utils/log.h"
#include "config.h"
#include "overlay/overlay.h"
#include "overlay/transitions.h"
#include "driver/drm_warpper.h"
#include "utils/timer.h"
#include "render/layer_animation.h"

#define STB_IMAGE_IMPLEMENTATION
#include "utils/stb_image.h"

// 渐变过渡，准备完成后无耗时操作，不需要使用worker。
void overlay_transition_fade(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){
    drm_warpper_queue_item_t* item;
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    overlay->request_abort = 0;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    for(int y = 0; y < OVERLAY_HEIGHT; y++){
        for(int x = 0; x < OVERLAY_WIDTH; x++){
            vaddr[x + y * OVERLAY_WIDTH] = params->background_color;
        }
    }

    if(params->image_addr){
        fbsrc.vaddr = params->image_addr;
        fbsrc.width = params->image_w;
        fbsrc.height = params->image_h;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->image_w;
        src_rect.h = params->image_h;

        fbdst.vaddr = vaddr;
        fbdst.width = OVERLAY_WIDTH;
        fbdst.height = OVERLAY_HEIGHT;

        dst_rect.x = OVERLAY_WIDTH / 2  - params->image_w / 2;
        dst_rect.y = OVERLAY_HEIGHT / 2 - params->image_h / 2;
        dst_rect.w = params->image_w;
        dst_rect.h = params->image_h;
        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    // 渐变到图层
    layer_animation_fade_in(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        0
    );

    // 全部遮住以后挂载video层
    prts_timer_handle_t init_handler;
    prts_timer_create(&init_handler,params->duration,0,1,middle_cb,userdata);
    
    // 渐变到透明
    layer_animation_fade_out(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        2 * params->duration
    );

}

// 贝塞尔函数移动过渡。 不需要使用worker
void overlay_transition_move(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){
    drm_warpper_queue_item_t* item;
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    overlay->request_abort = 0;

    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, SCREEN_WIDTH, 0);
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    for(int y = 0; y < OVERLAY_HEIGHT; y++){
        for(int x = 0; x < OVERLAY_WIDTH; x++){
            vaddr[x + y * OVERLAY_WIDTH] = params->background_color;
        }
    }

    if(params->image_addr){
        fbsrc.vaddr = params->image_addr;
        fbsrc.width = params->image_w;
        fbsrc.height = params->image_h;
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->image_w;
        src_rect.h = params->image_h;
        fbdst.vaddr = vaddr;
        fbdst.width = OVERLAY_WIDTH;
        fbdst.height = OVERLAY_HEIGHT;
        dst_rect.x = OVERLAY_WIDTH / 2  - params->image_w / 2;
        dst_rect.y = OVERLAY_HEIGHT / 2 - params->image_h / 2;
        dst_rect.w = params->image_w;
        dst_rect.h = params->image_h;
        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);


    layer_animation_ease_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        SCREEN_WIDTH, 0,
        0, 0,
        params->duration, 
        0
    );

    // 全部遮住以后挂载video层
    prts_timer_handle_t init_handler;
    prts_timer_create(&init_handler,params->duration,0,1,middle_cb,userdata);
    
    layer_animation_ease_in_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, 0,
        -SCREEN_WIDTH, 0,
        params->duration, 
        2 * params->duration
    );

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
    void (*middle_cb)(void *userdata,bool is_last);
    void* userdata;
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
    
    drm_warpper_set_layer_coord(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);
    drm_warpper_queue_item_t* item;
    drm_warpper_dequeue_free_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;


    // 状态转移 START
    swipe_draw_state_t draw_state;
    int draw_start_x;
    int draw_end_x;


    if (data->curr_frame < data->frames_per_stage){
        draw_state = SWIPE_DRAW_CONTENT;
        draw_start_x = data->bezeir_values[data->curr_frame];
        // double buffer，一次性需要推进两帧的内容
        if(data->curr_frame + 2 < data->frames_per_stage){
            draw_end_x = data->bezeir_values[data->curr_frame + 2];
        }
        else{
            draw_end_x = UI_WIDTH;
        }
    }
    else if (data->curr_frame < 2 * data->frames_per_stage){
        draw_state = SWIPE_DRAW_IDLE;
        if(!data->middle_cb_called){
            data->middle_cb(data->userdata, false);
            data->middle_cb_called = true;
        }
    }
    else{
        draw_state = SWIPE_DRAW_CLEAR;
        draw_start_x = data->bezeir_values[data->curr_frame - 2 * data->frames_per_stage];
        if(data->curr_frame + 2 < 3 * data->frames_per_stage){
            draw_end_x = data->bezeir_values[data->curr_frame - 2 * data->frames_per_stage + 2];
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

    drm_warpper_enqueue_display_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);
    data->curr_frame ++;
    if(data->curr_frame >= data->total_frames){
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
void overlay_transition_swipe(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){
    drm_warpper_queue_item_t* item;
    uint32_t* vaddr;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

    // 清空双缓冲buffer
    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);
    

    drm_warpper_mount_layer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0, &overlay->overlay_buf_1);


    static swipe_worker_data_t swipe_worker_data;
    swipe_worker_data.overlay = overlay;
    swipe_worker_data.curr_frame = 0;
    swipe_worker_data.frames_per_stage = params->duration / OVERLAY_ANIMATION_STEP_TIME;
    swipe_worker_data.total_frames = 3 * params->duration / OVERLAY_ANIMATION_STEP_TIME;
    swipe_worker_data.middle_cb = middle_cb;
    swipe_worker_data.userdata = userdata;
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
    if(params->type == TRANSITION_TYPE_NONE){
        return;
    }
    load_img_assets(params->image_path, &params->image_addr, &params->image_w, &params->image_h);
}

void overlay_transition_free_image(oltr_params_t* params){
    if(params->image_addr){
        free(params->image_addr);
        params->image_addr = NULL;
    }
}