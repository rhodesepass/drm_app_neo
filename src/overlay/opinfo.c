
#include "overlay/opinfo.h"
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "utils/timer.h"
#include "utils/stb_image.h"
#include "config.h"
#include "render/fbdraw.h"
#include "utils/cacheassets.h"
#include <src/misc/lv_text_private.h>
#include <stdint.h>
#include <unistd.h>

LV_FONT_DECLARE(ui_font_bebas_40);
LV_FONT_DECLARE(ui_font_bebas_bold_72);
LV_FONT_DECLARE(ui_font_sourcesans_reg_14);


#include <string.h>

void overlay_opinfo_show_image(overlay_t* overlay,olopinfo_params_t* params){
    drm_warpper_queue_item_t* item;
    fbdraw_fb_t fbdst,fbsrc;
    fbdraw_rect_t dst_rect,src_rect;

    overlay->request_abort = 0;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);

    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;

    if(params->image_addr){
        fbsrc.vaddr = params->image_addr;
        fbsrc.width = params->image_w;
        fbsrc.height = params->image_h;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->image_w;
        src_rect.h = params->image_h;

        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.w = params->image_w;
        dst_rect.h = params->image_h;

        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }
    
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);
    overlay->overlay_used = 1;

    layer_animation_ease_in_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, OVERLAY_HEIGHT,
        0, 0,
        params->duration, 0
    );
}

// 标记这个buffer里面这个元素有没有更新过
typedef struct{
    unsigned int operator_name : 1 ;
    unsigned int operator_code : 1 ;
    unsigned int barcode : 1 ;
    unsigned int staff_text : 1 ;
    unsigned int class_icon : 1 ;
    unsigned int aux_text : 1 ;
    unsigned int fade_color : 1;
    unsigned int rhodes:1;
    unsigned int arrow:1;
    unsigned int logo_fade:1;
    unsigned int ak_bar_swipe:1;
    unsigned int div_line_upper:1;
    unsigned int div_line_lower:1;


} arknights_overlay_update_t;

typedef enum {
    ANIMATION_EINK_FIRST_BLACK,
    ANIMATION_EINK_FIRST_WHITE,
    ANIMATION_EINK_SECOND_BLACK,
    ANIMATION_EINK_SECOND_WHITE,
    ANIMATION_EINK_IDLE,
    ANIMATION_EINK_CONTENT
} arknights_overlay_animation_eink_state_t;

typedef struct {
    overlay_t* overlay;
    olopinfo_params_t* params;

    int curr_frame;

    // codepoint index,codepoint count
    int operator_name_cpidx;
    int operator_name_cpcnt;

    int operator_code_cpidx;
    int operator_code_cpcnt;

    int stuff_text_cpidx;
    int stuff_text_cpcnt;

    int aux_text_cpidx;
    int aux_text_cpcnt;

    int color_fade_value;

    int logo_fade_value;

    int ak_bar_swipe_bezeir_values[OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT];

    arknights_overlay_animation_eink_state_t class_icon_state;
    arknights_overlay_animation_eink_state_t barcode_state;

    int div_line_bezeir_values[OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT];

    int arrow_y_value;

    // double buffer control.
    int curr_buffer;
    arknights_overlay_update_t buf1_update;
    arknights_overlay_update_t buf2_update;
} arknights_overlay_worker_data_t;


static void draw_color_fade(uint32_t* vaddr,int radius,uint32_t color){
    for(int x=0; x < radius; x++){
        for(int y=0; y < radius; y++){
            if(x+y > radius - 2){
                break;
            }
            uint8_t alpha = 255 - ((x+y)*255 / radius);
            int real_x = OVERLAY_WIDTH - x - 1;
            int real_y = OVERLAY_HEIGHT - y - 1;
            *((uint32_t *)(vaddr) + real_x + real_y * OVERLAY_WIDTH) = (color & 0x00FFFFFF) | (alpha << 24);

        }
    }
}
// 绘制arknights overlay的worker.
// 不处理跳帧了。
static void arknights_overlay_worker(void *userdata,int skipped_frames){
    arknights_overlay_worker_data_t* data = (arknights_overlay_worker_data_t*)userdata;

    // 是否要求我们退出
    if(data->overlay->request_abort){
        prts_timer_cancel(data->overlay->overlay_timer_handle);
        data->overlay->overlay_timer_handle = 0;
        log_debug("arknights overlay worker: request abort");
        return;
    }

    // =============  状态转移  ==================
    // == 文本 打字机效果 START

    // name
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_NAME_START_FRAME && data->operator_name_cpidx != data->operator_name_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_NAME_FRAME_PER_CODEPOINT == 0){
            data->operator_name_cpidx++;
            data->buf1_update.operator_name = 1;
            data->buf2_update.operator_name = 1;
        }
    }

    //code
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_CODE_START_FRAME && data->operator_code_cpidx != data->operator_code_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_CODE_FRAME_PER_CODEPOINT == 0){
            data->operator_code_cpidx++;
            data->buf1_update.operator_code = 1;
            data->buf2_update.operator_code = 1;
        }
    }

    //stuff text
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_START_FRAME && data->stuff_text_cpidx != data->stuff_text_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_STAFF_TEXT_FRAME_PER_CODEPOINT == 0){
            data->stuff_text_cpidx++;
            data->buf1_update.staff_text = 1;
            data->buf2_update.staff_text = 1;
        }
    }

    //aux text
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_AUX_TEXT_START_FRAME && data->aux_text_cpidx != data->aux_text_cpcnt){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_AUX_TEXT_FRAME_PER_CODEPOINT == 0){
            data->aux_text_cpidx++;
            data->buf1_update.aux_text = 1;
            data->buf2_update.aux_text = 1;
        }
    }

    // == 文本 打字机效果 END

    // == BARCODE 和 CLASSICON 的 Eink效果
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_CLASSICON_START_FRAME && data->class_icon_state != ANIMATION_EINK_CONTENT){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_CLASSICON_FRAME_PER_STATE == 0){
            data->class_icon_state++;
            data->buf1_update.class_icon = 1;
            data->buf2_update.class_icon = 1;
        }
    }

    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_BARCODE_START_FRAME && data->barcode_state != ANIMATION_EINK_CONTENT){
        if(data->curr_frame % OVERLAY_ANIMATION_OPINFO_BARCODE_FRAME_PER_STATE == 0){
            data->barcode_state++;
            data->buf1_update.barcode = 1;
            data->buf2_update.barcode = 1;
        }
    }
    // == BARCODE 和 CLASSICON 的 Eink效果 end ==

    // == color fade 和 logo fade start ==
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_COLOR_FADE_START_FRAME && data->color_fade_value < OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE){
        data->color_fade_value += OVERLAY_ANIMATION_OPINFO_COLOR_FADE_VALUE_PER_FRAME;
        if(data->color_fade_value >= OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE){
            data->color_fade_value = OVERLAY_ANIMATION_OPINFO_COLOR_FADE_END_VALUE;
        }
        data->buf1_update.fade_color = 1;
        data->buf2_update.fade_color = 1;
    }
    // == color fade 和 logo fade end ==

    // == logo swipe start
    if(data->curr_frame >= OVERLAY_ANIMATION_OPINFO_LOGO_FADE_START_FRAME && data->logo_fade_value < 255){
        data->logo_fade_value += OVERLAY_ANIMATION_OPINFO_LOGO_FADE_VALUE_PER_FRAME;
        if(data->logo_fade_value >= 255){
            data->logo_fade_value = 255;
        }
        data->buf1_update.logo_fade = 1;
        data->buf2_update.logo_fade = 1;
        // logo 和 colorfade是冲突的 
        // colorfade 也需要重画
        data->buf1_update.fade_color = 1;
        data->buf2_update.fade_color = 1;
    }
    // == logo swipe end ==

    // == ak bar swipe start

    int ak_bar_swipe_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_START_FRAME;
    if (ak_bar_swipe_frame >= 0 && ak_bar_swipe_frame < OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT){
        data->buf1_update.ak_bar_swipe = 1;
        data->buf2_update.ak_bar_swipe = 1;
    }

    // division line start

    int div_line_upper_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_LINE_UPPER_START_FRAME;
    if (div_line_upper_frame >= 0 && div_line_upper_frame < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT){
        data->buf1_update.div_line_upper = 1;
        data->buf2_update.div_line_upper = 1;
    }

    int div_line_lower_frame = data->curr_frame - OVERLAY_ANIMATION_OPINFO_LINE_LOWER_START_FRAME;
    if (div_line_lower_frame >= 0 && div_line_lower_frame < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT){
        data->buf1_update.div_line_lower = 1;
        data->buf2_update.div_line_lower = 1;
    }
    // log_trace("arknights_overlay_worker: curr_frame=%d", data->curr_frame);
    // log_trace("operator_name_cpidx=%d,operator_name_cpcnt=%d", data->operator_name_cpidx, data->operator_name_cpcnt);
    // log_trace("operator_code_cpidx=%d,operator_code_cpcnt=%d", data->operator_code_cpidx, data->operator_code_cpcnt);
    // log_trace("stuff_text_cpidx=%d,stuff_text_cpcnt=%d", data->stuff_text_cpidx, data->stuff_text_cpcnt);
    // log_trace("aux_text_cpidx=%d,aux_text_cpcnt=%d", data->aux_text_cpidx, data->aux_text_cpcnt);

    // log_trace("update.operator_name=%d,update.operator_code=%d,update.staff_text=%d,update.aux_text=%d,update.barcode=%d,update.class_icon=%d,update.fade_color=%d,update.logo_fade=%d", 
    //     data->buf1_update.operator_name,
    //     data->buf1_update.operator_code,
    //     data->buf1_update.staff_text,
    //     data->buf1_update.aux_text,
    //     data->buf1_update.barcode,
    //     data->buf1_update.class_icon,
    //     data->buf1_update.fade_color,data->buf1_update.logo_fade);

    // log_trace("logo addr: %p", data->params->logo_addr);
    // log_trace("logo w: %d", data->params->logo_w);
    // log_trace("logo h: %d", data->params->logo_h);
    // log_trace("class addr: %p", data->params->class_addr);
    // log_trace("class w: %d", data->params->class_w);
    // log_trace("class h: %d", data->params->class_h);

    // log_trace("fade color radius:%d",data->color_fade_value);

    // =========== 绘制 ================

    int asset_w, asset_h;
    uint8_t* asset_addr;
    arknights_overlay_update_t * update;
    if(data->curr_buffer == 0){
        update = &data->buf1_update;
    }
    else{
        update = &data->buf2_update;
    }

    drm_warpper_queue_item_t* item;
    drm_warpper_dequeue_free_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    fbdraw_fb_t fbdst;
    fbdraw_fb_t fbsrc;
    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;


    fbdraw_rect_t dst_rect;
    fbdraw_rect_t src_rect;

    olopinfo_params_t* params = data->params;

    // == 文本 start ==
    // name
    if(update->operator_name){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect, 
            params->operator_name, 
            &ui_font_bebas_40, 
            0xFFFFFFFF,0, 
            data->operator_name_cpidx, data->operator_name_cpidx + 1
        );
        update->operator_name = 0;
    }
    // code
    if(update->operator_code){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect, 
            params->operator_code, 
            &ui_font_sourcesans_reg_14, 
            0xFFFFFFFF,0, 
            data->operator_code_cpidx, data->operator_code_cpidx + 1
        );
        update->operator_code = 0;
    }
    //stuff text
    if(update->staff_text){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect, 
            params->staff_text, 
            &ui_font_sourcesans_reg_14, 
            0xFFFFFFFF,0, 
            data->stuff_text_cpidx, data->stuff_text_cpidx + 1
        );
        update->staff_text = 0;
    }
    //aux text
    if(update->aux_text){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y;
        dst_rect.w = OVERLAY_WIDTH;
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_text_range(
            &fbdst, &dst_rect, 
            params->aux_text, 
            &ui_font_sourcesans_reg_14, 
            0xFFFFFFFF,14, 
            data->aux_text_cpidx, data->aux_text_cpidx + 1
        );
        update->aux_text = 0;
    }
    // == 文本 end ==

    // == BARCODE 和 CLASSICON 的 Eink效果 start ==
    if(update->barcode){
        dst_rect.x = 1;
        dst_rect.y = OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y;
        dst_rect.w = OVERLAY_ARKNIGHTS_BARCODE_WIDTH;
        dst_rect.h = OVERLAY_ARKNIGHTS_BARCODE_HEIGHT;

        if (data->barcode_state == ANIMATION_EINK_FIRST_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->barcode_state == ANIMATION_EINK_FIRST_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->barcode_state == ANIMATION_EINK_SECOND_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->barcode_state == ANIMATION_EINK_SECOND_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->barcode_state == ANIMATION_EINK_IDLE){
            //does nothing
        }   
        else{
            fbdraw_barcode_rot90(&fbdst, &dst_rect, params->barcode_text, &ui_font_sourcesans_reg_14);
        }

        update->barcode = 0;
    }

    if(update->class_icon){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y;
        dst_rect.w = params->class_w;
        dst_rect.h = params->class_h;

        if (data->class_icon_state == ANIMATION_EINK_FIRST_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->class_icon_state == ANIMATION_EINK_FIRST_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->class_icon_state == ANIMATION_EINK_SECOND_BLACK){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFF000000);
        }
        else if (data->class_icon_state == ANIMATION_EINK_SECOND_WHITE){
            fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);
        }
        else if (data->class_icon_state == ANIMATION_EINK_IDLE){
            //does nothing
        }   
        else{
            fbsrc.vaddr = (uint32_t*) params->class_addr;
            fbsrc.width = params->class_w;
            fbsrc.height = params->class_h;

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.w = params->class_w;
            src_rect.h = params->class_h;
    
            fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
        }

        update->class_icon = 0;
    }
    // == BARCODE 和 CLASSICON 的 Eink效果 end ==

    // == color fade start
    if(update->fade_color){
        draw_color_fade(vaddr, data->color_fade_value, params->color);
        update->fade_color = 0;
    }

    // == logo fade start
    if(update->logo_fade){
        fbsrc.vaddr = (uint32_t*)params->logo_addr;
        fbsrc.width = params->logo_w;
        fbsrc.height = params->logo_h;
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = params->logo_w;
        src_rect.h = params->logo_h;
        dst_rect.x = OVERLAY_WIDTH - data->params->logo_w - 10;
        dst_rect.y = OVERLAY_HEIGHT - data->params->logo_h - 10;
        dst_rect.w = params->logo_w;
        dst_rect.h = params->logo_h;
        fbdraw_alpha_opacity_rect(&fbsrc, &fbdst, &src_rect, &dst_rect, data->logo_fade_value);
        update->logo_fade = 0;
    }

    if (update->ak_bar_swipe){
        cacheassets_get_asset_from_global(CACHE_ASSETS_AK_BAR, &asset_w, &asset_h, &asset_addr);
        fbsrc.vaddr = (uint32_t*)asset_addr;
        fbsrc.width = asset_w;
        fbsrc.height = asset_h;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = data->ak_bar_swipe_bezeir_values[ak_bar_swipe_frame];
        src_rect.h = asset_h;

        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y;
        dst_rect.w = data->ak_bar_swipe_bezeir_values[ak_bar_swipe_frame];
        dst_rect.h = OVERLAY_HEIGHT;

        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

        update->ak_bar_swipe = 0;
    }

    if(update->div_line_upper){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y;
        dst_rect.w = data->div_line_bezeir_values[div_line_upper_frame];
        dst_rect.h = 1;
        fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

        update->div_line_upper = 0;
    }

    if(update->div_line_lower){
        dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
        dst_rect.y = OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y;
        dst_rect.w = data->div_line_bezeir_values[div_line_lower_frame];
        dst_rect.h = 1;
        fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

        update->div_line_lower = 0;
    }

    // ARROWS. it always redraw

    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_ARROW, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = data->arrow_y_value;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y + (asset_h - data->arrow_y_value);
    dst_rect.w = asset_w;
    dst_rect.h = data->arrow_y_value;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    src_rect.x = 0;
    src_rect.y = data->arrow_y_value;
    src_rect.w = asset_w;
    src_rect.h = asset_h - data->arrow_y_value;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h - data->arrow_y_value;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    data->arrow_y_value -= OVERLAY_ANIMATION_OPINFO_ARROW_Y_INCR_PER_FRAME;
    if(data->arrow_y_value <= 0){
        data->arrow_y_value = asset_h;
    }

    drm_warpper_enqueue_display_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);
    data->curr_buffer = !data->curr_buffer;
    data->curr_frame++;
}

// 定时器回调。来自普瑞塞斯 的 rt 启动的 sigev_thread 线程。
static void arknights_overlay_worker_timer_cb(void *userdata,bool is_last){
    arknights_overlay_worker_data_t* data = (arknights_overlay_worker_data_t*)userdata;
    overlay_worker_schedule(data->overlay,arknights_overlay_worker,data);
}

static void init_template_arknights_overlay(uint32_t* vaddr, olopinfo_params_t* params){
    log_info("init_template_arknights: rhodes_text=[%s]", params->rhodes_text);

    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);


    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;


    int asset_w,asset_h;
    uint8_t* asset_addr;

    // TOP_LEFT_RECT
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RECT, &asset_w, &asset_h, &asset_addr);

    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_ARKNIGHTS_RECT_OFFSET_X;
    dst_rect.y = 0;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // BTM_LEFT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_BTM_LEFT_BAR, &asset_w, &asset_h, &asset_addr);
    
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = 0;
    dst_rect.y = OVERLAY_HEIGHT - asset_h;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_RIGHT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_BAR, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = 0;
    dst_rect.w = asset_w;
    dst_rect.h = asset_h;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_LEFT_RHODES
    if (params->rhodes_text[0] != '\0') {
        // 用户自定义文字替代 logo（顺时针旋转 +90° 显示，72px Bold）
        dst_rect.x = 0;
        dst_rect.y = 5;
        dst_rect.w = 67;
        dst_rect.h = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y - 5;
        fbdraw_text_rot90(&fbdst, &dst_rect, params->rhodes_text, &ui_font_bebas_bold_72, 0xFFFFFFFF);
    } else {
        // 默认缓存图
        cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RHODES, &asset_w, &asset_h, &asset_addr);
        fbsrc.vaddr = (uint32_t*)asset_addr;
        fbsrc.width = asset_w;
        fbsrc.height = asset_h;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = asset_w;
        src_rect.h = asset_h;

        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.w = asset_w;
        dst_rect.h = asset_h;

        fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
    }

}

void overlay_opinfo_show_arknights(overlay_t* overlay,olopinfo_params_t* params){
    drm_warpper_queue_item_t* item;
    uint32_t* vaddr;

    log_info("overlay_opinfo_show_arknights");

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

    // 清空双缓冲buffer
    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    init_template_arknights_overlay(vaddr, params);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    init_template_arknights_overlay(vaddr, params);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    static arknights_overlay_worker_data_t data;
    memset(&data, 0, sizeof(arknights_overlay_worker_data_t));
    data.overlay = overlay;
    data.params = params;
    data.operator_name_cpcnt = lv_text_get_encoded_length(params->operator_name);
    data.operator_code_cpcnt = lv_text_get_encoded_length(params->operator_code);
    data.stuff_text_cpcnt = lv_text_get_encoded_length(params->staff_text);
    data.aux_text_cpcnt = lv_text_get_encoded_length(params->aux_text);

    int h,w;
    uint8_t* addr;
    cacheassets_get_asset_from_global(CACHE_ASSETS_AK_BAR, &w, &h, &addr);

    int32_t ctlx1 = LV_BEZIER_VAL_FLOAT(0.42);
    int32_t ctly1 = LV_BEZIER_VAL_FLOAT(0);
    int32_t ctx2 = LV_BEZIER_VAL_FLOAT(0.58);
    int32_t cty2 = LV_BEZIER_VAL_FLOAT(1);

    for(int i = 0; i < OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT; i++){
        uint32_t t = lv_map(i, 0, OVERLAY_ANIMATION_OPINFO_AK_BAR_SWIPE_FRAME_COUNT, 0, LV_BEZIER_VAL_MAX);
        int32_t step = lv_cubic_bezier(t, ctlx1, ctly1, ctx2, cty2);
        int32_t new_value;
        new_value = step * w;
        new_value = new_value >> LV_BEZIER_VAL_SHIFT;
        data.ak_bar_swipe_bezeir_values[i] = new_value;
    }

    for(int i = 0; i < OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT; i++){
        uint32_t t = lv_map(i, 0, OVERLAY_ANIMATION_OPINFO_LINE_FRAME_COUNT, 0, LV_BEZIER_VAL_MAX);
        int32_t step = lv_cubic_bezier(t, ctlx1, ctly1, ctx2, cty2);
        int32_t new_value;
        new_value = step * OVERLAY_ARKNIGHTS_LINE_WIDTH;
        new_value = new_value >> LV_BEZIER_VAL_SHIFT;
        data.div_line_bezeir_values[i] = new_value;
    }

    overlay->request_abort = 0;
    overlay->overlay_used = 1;

    prts_timer_create(
        &overlay->overlay_timer_handle  ,
        0,
        OVERLAY_ANIMATION_STEP_TIME,
        -1,
        arknights_overlay_worker_timer_cb,
        &data
    ); 


    layer_animation_ease_in_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, OVERLAY_HEIGHT,
        0, 0,
        1 * 1000 * 1000, 0
    );
}


void overlay_opinfo_load_image(olopinfo_params_t* params){
    if(params->type == OPINFO_TYPE_NONE){
        return;
    }
    if(params->type == OPINFO_TYPE_IMAGE){
        load_img_assets(params->image_path, &params->image_addr, &params->image_w, &params->image_h);
        log_debug("loaded image: %s, w: %d, h: %d", params->image_path, params->image_w, params->image_h);
    }
    else if(params->type == OPINFO_TYPE_ARKNIGHTS){
        load_img_assets(params->class_path, &params->class_addr, &params->class_w, &params->class_h);
        load_img_assets(params->logo_path, &params->logo_addr, &params->logo_w, &params->logo_h);
        log_debug("loaded class: %s, w: %d, h: %d", params->class_path, params->class_w, params->class_h);
        log_debug("loaded logo: %s, w: %d, h: %d", params->logo_path, params->logo_w, params->logo_h);
    }
}

void overlay_opinfo_free_image(olopinfo_params_t* params){
    if(params->type == OPINFO_TYPE_NONE){
        return;
    }
    if(params->type == OPINFO_TYPE_IMAGE){
        if(params->image_addr){
            free(params->image_addr);
            params->image_addr = NULL;
            log_debug("freed image: %s", params->image_path);
        }
    }
    else if(params->type == OPINFO_TYPE_ARKNIGHTS){
        if(params->class_addr){
            free(params->class_addr);
            params->class_addr = NULL;
            log_debug("freed class: %s", params->class_path);
        }
        if(params->logo_addr){
            free(params->logo_addr);
            params->logo_addr = NULL;
            log_debug("freed logo: %s", params->logo_path);
        }
    }
}