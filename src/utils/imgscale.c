#include "imgscale.h"

#include <stdlib.h>
#include <string.h>

int imgscale_upscale_nn_rgba(uint32_t **pixels, int *w, int *h, int factor)
{
    if (!pixels || !*pixels || !w || !h)
        return -1;
    if (factor <= 1)
        return 0;

    int src_w = *w;
    int src_h = *h;
    int dst_w = src_w * factor;
    int dst_h = src_h * factor;

    uint32_t *dst = malloc((size_t)dst_w * dst_h * sizeof(uint32_t));
    if (!dst)
        return -1;

    const uint32_t *src = *pixels;
    for (int y = 0; y < src_h; y++) {
        const uint32_t *src_row = src + (size_t)y * src_w;
        uint32_t *dst_row = dst + (size_t)y * factor * dst_w;
        for (int x = 0; x < src_w; x++) {
            uint32_t px = src_row[x];
            for (int fx = 0; fx < factor; fx++)
                dst_row[x * factor + fx] = px;
        }
        for (int fy = 1; fy < factor; fy++)
            memcpy(dst + ((size_t)y * factor + fy) * dst_w, dst_row,
                   (size_t)dst_w * sizeof(uint32_t));
    }

    free(*pixels);
    *pixels = dst;
    *w = dst_w;
    *h = dst_h;
    return 0;
}

int imgscale_downscale_nn_rgba(uint32_t **pixels, int *w, int *h, int factor)
{
    if (!pixels || !*pixels || !w || !h)
        return -1;
    if (factor <= 1)
        return 0;

    int src_w = *w;
    int src_h = *h;
    int dst_w = src_w / factor;
    int dst_h = src_h / factor;
    if (dst_w <= 0 || dst_h <= 0)
        return -1;

    uint32_t *dst = malloc((size_t)dst_w * dst_h * sizeof(uint32_t));
    if (!dst)
        return -1;

    const uint32_t *src = *pixels;
    for (int y = 0; y < dst_h; y++) {
        const uint32_t *src_row = src + (size_t)y * factor * src_w;
        uint32_t *dst_row = dst + (size_t)y * dst_w;
        for (int x = 0; x < dst_w; x++)
            dst_row[x] = src_row[x * factor];
    }

    free(*pixels);
    *pixels = dst;
    *w = dst_w;
    *h = dst_h;
    return 0;
}

int imgscale_rescale_nn_rgba(uint32_t **pixels, int *w, int *h, int up, int down)
{
    int rc = imgscale_upscale_nn_rgba(pixels, w, h, up);
    if (rc != 0)
        return rc;
    return imgscale_downscale_nn_rgba(pixels, w, h, down);
}
