// 说起来，我们不是有一块video层的内存区域单纯是拿来做modeset的吗
// 干脆拿来做缓存算了

#include "utils/cacheassets.h"
#include "utils/stb_image.h"
#include "utils/log.h"
#include "config.h"

// 素材按 2x(720) 基准出图。当前档 UI_SCALE: 2x 原样, 1x 最近邻取半。
#define CACHEASSETS_SRC_SCALE 2

static cacheassets_t * g_cacheassets_ptr = NULL;
void cacheassets_init(cacheassets_t* cacheassets,uint8_t* base_addr,int max_size){
    g_cacheassets_ptr = cacheassets;
    cacheassets->base_addr = base_addr;
    cacheassets->curr_size = 0;
    cacheassets->max_size = max_size;
    for(int i = 0; i < CACHE_ASSETS_MAX_ASSET_MAX; i++){
        cacheassets->asset_addr[i] = NULL;
    }
    log_info("==> Cacheassets Initalized!");
}

void cacheassets_put_asset(cacheassets_t* cacheassets,cacheasset_asset_id_t asset_id,char* image_path){
    int w,h,c;
    uint8_t* pixdata = stbi_load(image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", image_path);
        return;
    }
    // 2x 素材按当前档缩放: dst = src * UI_SCALE / 2 (UI_SCALE=2 原样, 1 取半)
    int dst_w = w * UI_SCALE / CACHEASSETS_SRC_SCALE;
    int dst_h = h * UI_SCALE / CACHEASSETS_SRC_SCALE;
    int size = dst_w * dst_h * 4;
    if(size > cacheassets->max_size - cacheassets->curr_size){
        log_error("image size is too large: %s", image_path);
        stbi_image_free(pixdata);
        pixdata = NULL;
        return;
    }

    uint32_t* dst = (uint32_t *)(cacheassets->base_addr + cacheassets->curr_size);
    for(int dy = 0; dy < dst_h; dy++){
        int sy = dy * CACHEASSETS_SRC_SCALE / UI_SCALE;
        for(int dx = 0; dx < dst_w; dx++){
            int sx = dx * CACHEASSETS_SRC_SCALE / UI_SCALE;
            uint32_t bgra_pixel = *((uint32_t *)(pixdata) + sx + sy * w);
            uint32_t rgb_pixel = (bgra_pixel & 0x000000FF) << 16 | (bgra_pixel & 0x0000FF00) | (bgra_pixel & 0x00FF0000) >> 16 | (bgra_pixel & 0xFF000000);
            dst[dx + dy * dst_w] = rgb_pixel;
        }
    }
    cacheassets->asset_w[asset_id] = dst_w;
    cacheassets->asset_h[asset_id] = dst_h;
    cacheassets->asset_addr[asset_id] = cacheassets->base_addr + cacheassets->curr_size;
    cacheassets->curr_size += size;
    stbi_image_free(pixdata);
    pixdata = NULL;
    log_info("Cached asset: %s, size: %d", image_path, size);
}

void cacheassets_get_asset(cacheassets_t* cacheassets,cacheasset_asset_id_t asset_id,int* w,int* h,uint8_t** pixdata){
    *w = cacheassets->asset_w[asset_id];
    *h = cacheassets->asset_h[asset_id];
    *pixdata = cacheassets->asset_addr[asset_id];
}

void cacheassets_get_asset_from_global(cacheasset_asset_id_t asset_id,int* w,int* h,uint8_t** pixdata){
    *w = g_cacheassets_ptr->asset_w[asset_id];
    *h = g_cacheassets_ptr->asset_h[asset_id];
    *pixdata = g_cacheassets_ptr->asset_addr[asset_id];
}