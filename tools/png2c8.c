// png2c8 — C8(256 色调色板)烘焙段与 cacheasset 索引素材的离线生成。
//
// 一次调用完成全部输出,保证调色板与索引图永远一致(不用解析自己生成的头):
//   png2c8 <out_baked.h> <out_dir> in1.png in2.png ...
// 对每张 inN.png 输出 <out_dir>/<name>_2x.c8(原样)与 <name>_1x.c8(2x2 取半),
// 并生成 src/render/c8pal_baked.h(96 项:固定前缀 33 + 素材联合聚类 63)。
//
// .c8 格式: magic "C8A\0" + u16le width + u16le height + uint8 索引位图
// (索引是全局绝对索引,运行时零转换;0 = 全透明)
//
// 量化策略:
//   - 调色板: 全部素材(全分辨率)联合 median-cut(RGBA 4 维,alpha 权重 x2),
//     半透明像素参与聚类 -> AA 边缘可以落到带 alpha 的表项,DEBE 真混合
//   - 索引图: Floyd-Steinberg 误差扩散(只扩 RGB,透明像素不吃不吐误差),
//     可用全部 96 项(含白/黑 ramp 和灰阶,不限于素材聚类段)
//   - 抖动必须在目标分辨率上做(先缩后抖),1x 用 2x2 alpha 加权盒式取半
//
// 固定前缀布局与 src/config.h 的 C8PAL_* 注释一致,改动要两边同步。
//
// host 编译: gcc -O2 -o png2c8 png2c8.c -lm ;全量重生成见本目录 gen_c8.sh

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../src/utils/stb_image.h"

#define BAKED_COUNT 96
#define ASSETS_BASE 33
#define ASSETS_COUNT 63

static uint32_t g_pal[BAKED_COUNT]; // 0xAARRGGBB

// ---- 固定前缀(与 config.h / c8pal.c 的约定一致) ----
static void build_fixed_prefix(void)
{
    static const uint8_t ramp_a[8] = {224, 192, 160, 128, 96, 64, 40, 16};
    g_pal[0] = 0x00000000;
    g_pal[1] = 0xFF000000;
    g_pal[2] = 0xFFFFFFFF;
    for(int i = 0; i < 8; i++){
        g_pal[3 + i] = ((uint32_t)ramp_a[i] << 24) | 0x00FFFFFF;
        g_pal[11 + i] = ((uint32_t)ramp_a[i] << 24);
    }
    for(int i = 0; i < 14; i++){
        uint32_t v = (uint32_t)((255 * (i + 1) + 7) / 15);
        g_pal[19 + i] = 0xFF000000 | (v << 16) | (v << 8) | v;
    }
}

// ---- median-cut(与 src/render/c8pal.c 同款,自含复制) ----

typedef struct { int start, len; } qbox_t;
static int g_sort_ch;

static int sample_cmp(const void* pa, const void* pb)
{
    const uint8_t* a = (const uint8_t*)pa;
    const uint8_t* b = (const uint8_t*)pb;
    if(a[g_sort_ch] != b[g_sort_ch])
        return (int)a[g_sort_ch] - (int)b[g_sort_ch];
    return memcmp(a, b, 4);
}

static int median_cut(uint8_t* samples, int count, int max_n, uint32_t* out_pal)
{
    qbox_t boxes[256];
    int nbox = 1;
    boxes[0].start = 0;
    boxes[0].len = count;

    while(nbox < max_n){
        int pick = -1, pick_ch = 0, pick_score = 0;
        for(int i = 0; i < nbox; i++){
            if(boxes[i].len < 2) continue;
            int lo[4] = {255, 255, 255, 255}, hi[4] = {0, 0, 0, 0};
            const uint8_t* p = samples + (size_t)boxes[i].start * 4;
            for(int j = 0; j < boxes[i].len; j++)
                for(int c = 0; c < 4; c++){
                    int v = p[j * 4 + c];
                    if(v < lo[c]) lo[c] = v;
                    if(v > hi[c]) hi[c] = v;
                }
            for(int c = 0; c < 4; c++){
                int score = (hi[c] - lo[c]) * (c == 3 ? 2 : 1);
                if(score > pick_score){
                    pick_score = score;
                    pick = i;
                    pick_ch = c;
                }
            }
        }
        if(pick < 0) break;

        qbox_t* bx = &boxes[pick];
        g_sort_ch = pick_ch;
        qsort(samples + (size_t)bx->start * 4, bx->len, 4, sample_cmp);
        int half = bx->len / 2;
        boxes[nbox].start = bx->start + half;
        boxes[nbox].len = bx->len - half;
        bx->len = half;
        nbox++;
    }

    for(int i = 0; i < nbox; i++){
        uint64_t sum[4] = {0, 0, 0, 0};
        const uint8_t* p = samples + (size_t)boxes[i].start * 4;
        for(int j = 0; j < boxes[i].len; j++)
            for(int c = 0; c < 4; c++)
                sum[c] += p[j * 4 + c];
        uint32_t n = (uint32_t)boxes[i].len;
        uint32_t r = (uint32_t)((sum[0] + n / 2) / n);
        uint32_t g = (uint32_t)((sum[1] + n / 2) / n);
        uint32_t b = (uint32_t)((sum[2] + n / 2) / n);
        uint32_t a = (uint32_t)((sum[3] + n / 2) / n);
        out_pal[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    return nbox;
}

// ---- 最近表项(加权距离与运行时 c8pal 一致) ----

static int nearest(int r, int g, int b, int a, int n)
{
    uint32_t best_d = 0xFFFFFFFFu;
    int best = 0;
    for(int i = 0; i < n; i++){
        uint32_t c = g_pal[i];
        int dr = (int)((c >> 16) & 0xFF) - r;
        int dg = (int)((c >> 8) & 0xFF) - g;
        int db = (int)(c & 0xFF) - b;
        int da = (int)(c >> 24) - a;
        uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db + 4 * da * da);
        if(d < best_d){
            best_d = d;
            best = i;
        }
    }
    return best;
}

static int clamp255(int v){ return v < 0 ? 0 : (v > 255 ? 255 : v); }

// ---- 图片装载(RGBA float,half 时 2x2 alpha 加权盒式取半) ----

typedef struct {
    int w, h;
    float* rgba; // w*h*4
} img_t;

static int img_load(const char* path, int half, img_t* out)
{
    int sw, sh, c;
    uint8_t* src = stbi_load(path, &sw, &sh, &c, 4);
    if(!src) return -1;

    int w = half ? sw / 2 : sw;
    int h = half ? sh / 2 : sh;
    float* d = malloc(sizeof(float) * w * h * 4);
    for(int y = 0; y < h; y++){
        for(int x = 0; x < w; x++){
            float r, g, b, a;
            if(half){
                float ra = 0, ga = 0, ba = 0, asum = 0;
                for(int dy = 0; dy < 2; dy++)
                    for(int dx = 0; dx < 2; dx++){
                        const uint8_t* p = src + (((size_t)y * 2 + dy) * sw + x * 2 + dx) * 4;
                        ra += (float)p[0] * p[3];
                        ga += (float)p[1] * p[3];
                        ba += (float)p[2] * p[3];
                        asum += p[3];
                    }
                a = asum / 4.0f;
                r = asum > 0 ? ra / asum : 0;
                g = asum > 0 ? ga / asum : 0;
                b = asum > 0 ? ba / asum : 0;
            } else {
                const uint8_t* p = src + ((size_t)y * sw + x) * 4;
                r = p[0]; g = p[1]; b = p[2]; a = p[3];
            }
            float* q = d + ((size_t)y * w + x) * 4;
            q[0] = r; q[1] = g; q[2] = b; q[3] = a;
        }
    }
    stbi_image_free(src);
    out->w = w;
    out->h = h;
    out->rgba = d;
    return 0;
}

// ---- F-S 抖动出绝对索引图并写 .c8 ----

static int write_c8(const char* path, const img_t* im)
{
    int w = im->w, h = im->h;
    uint8_t* idx = malloc((size_t)w * h);
    float* err = calloc((size_t)(w + 2) * 2 * 3, sizeof(float));
    float* cur = err + 3;
    float* nxt = err + (w + 2) * 3 + 3;

    for(int y = 0; y < h; y++){
        memset(nxt - 3, 0, sizeof(float) * (w + 2) * 3);
        for(int x = 0; x < w; x++){
            const float* p = im->rgba + ((size_t)y * w + x) * 4;
            int a = (int)(p[3] + 0.5f);
            if(a < 8){
                idx[(size_t)y * w + x] = 0; // 全透明
                continue;
            }
            int r = clamp255((int)(p[0] + cur[x * 3 + 0] + 0.5f));
            int g = clamp255((int)(p[1] + cur[x * 3 + 1] + 0.5f));
            int b = clamp255((int)(p[2] + cur[x * 3 + 2] + 0.5f));

            int i = nearest(r, g, b, a, BAKED_COUNT);
            idx[(size_t)y * w + x] = (uint8_t)i;

            float er = (float)r - (float)((g_pal[i] >> 16) & 0xFF);
            float eg = (float)g - (float)((g_pal[i] >> 8) & 0xFF);
            float eb = (float)b - (float)(g_pal[i] & 0xFF);
            cur[(x + 1) * 3 + 0] += er * 7 / 16;
            cur[(x + 1) * 3 + 1] += eg * 7 / 16;
            cur[(x + 1) * 3 + 2] += eb * 7 / 16;
            nxt[(x - 1) * 3 + 0] += er * 3 / 16;
            nxt[(x - 1) * 3 + 1] += eg * 3 / 16;
            nxt[(x - 1) * 3 + 2] += eb * 3 / 16;
            nxt[x * 3 + 0] += er * 5 / 16;
            nxt[x * 3 + 1] += eg * 5 / 16;
            nxt[x * 3 + 2] += eb * 5 / 16;
            nxt[(x + 1) * 3 + 0] += er / 16;
            nxt[(x + 1) * 3 + 1] += eg / 16;
            nxt[(x + 1) * 3 + 2] += eb / 16;
        }
        float* t = cur;
        cur = nxt;
        nxt = t;
    }
    free(err);

    FILE* f = fopen(path, "wb");
    if(!f){
        free(idx);
        return -1;
    }
    uint8_t hdr[8] = {'C', '8', 'A', 0,
                      (uint8_t)(w & 0xFF), (uint8_t)(w >> 8),
                      (uint8_t)(h & 0xFF), (uint8_t)(h >> 8)};
    int ok = fwrite(hdr, 1, 8, f) == 8
          && fwrite(idx, 1, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    free(idx);
    return ok ? 0 : -1;
}

static const char* basename_noext(const char* path, char* buf, size_t cap)
{
    const char* s = strrchr(path, '/');
    s = s ? s + 1 : path;
    snprintf(buf, cap, "%s", s);
    char* dot = strrchr(buf, '.');
    if(dot) *dot = 0;
    return buf;
}

int main(int argc, char** argv)
{
    if(argc < 4){
        fprintf(stderr, "usage: %s <out_baked.h> <out_dir> in1.png [in2.png ...]\n", argv[0]);
        return 1;
    }
    const char* out_hdr = argv[1];
    const char* out_dir = argv[2];
    const int nimg = argc - 3;

    build_fixed_prefix();

    // 1) 全部素材(全分辨率)联合采样 -> 聚 63 色
    img_t* full = malloc(sizeof(img_t) * nimg);
    size_t total_px = 0;
    for(int i = 0; i < nimg; i++){
        if(img_load(argv[3 + i], 0, &full[i]) != 0){
            fprintf(stderr, "failed to load %s\n", argv[3 + i]);
            return 1;
        }
        total_px += (size_t)full[i].w * full[i].h;
    }

    const int max_samples = 1 << 18;
    int stride = (int)(total_px / max_samples) + 1;
    uint8_t* samples = malloc((total_px / stride + 16) * 4);
    int count = 0;
    size_t gidx = 0;
    for(int i = 0; i < nimg; i++){
        size_t n = (size_t)full[i].w * full[i].h;
        for(size_t j = 0; j < n; j++, gidx++){
            if(gidx % stride) continue;
            const float* p = full[i].rgba + j * 4;
            if(p[3] < 8) continue;
            samples[count * 4 + 0] = (uint8_t)(p[0] + 0.5f);
            samples[count * 4 + 1] = (uint8_t)(p[1] + 0.5f);
            samples[count * 4 + 2] = (uint8_t)(p[2] + 0.5f);
            samples[count * 4 + 3] = (uint8_t)(p[3] + 0.5f);
            count++;
        }
    }
    uint32_t asset_pal[ASSETS_COUNT];
    int nasset = median_cut(samples, count, ASSETS_COUNT, asset_pal);
    for(int i = 0; i < ASSETS_COUNT; i++)
        g_pal[ASSETS_BASE + i] = i < nasset ? asset_pal[i] : 0xFF808080;
    printf("palette: %d asset colors (of %d slots)\n", nasset, ASSETS_COUNT);

    // 2) 逐素材出 _2x/_1x 索引图
    char name[128], path[512];
    for(int i = 0; i < nimg; i++){
        basename_noext(argv[3 + i], name, sizeof(name));

        snprintf(path, sizeof(path), "%s/%s_2x.c8", out_dir, name);
        if(write_c8(path, &full[i]) != 0){
            fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
        printf("  %s (%dx%d)\n", path, full[i].w, full[i].h);

        img_t halfimg;
        if(img_load(argv[3 + i], 1, &halfimg) != 0) return 1;
        snprintf(path, sizeof(path), "%s/%s_1x.c8", out_dir, name);
        if(write_c8(path, &halfimg) != 0){
            fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
        printf("  %s (%dx%d)\n", path, halfimg.w, halfimg.h);
        free(halfimg.rgba);
    }

    // 3) baked.h
    FILE* f = fopen(out_hdr, "w");
    if(!f){
        fprintf(stderr, "failed to open %s\n", out_hdr);
        return 1;
    }
    fprintf(f, "#pragma once\n");
    fprintf(f, "// 由 tools/png2c8 生成,勿手改;重生成: tools/gen_c8.sh\n");
    fprintf(f, "// 布局: 0 透明 | 1 黑 | 2 白 | 3..10 白 ramp | 11..18 黑 ramp |\n");
    fprintf(f, "//       19..32 灰阶 | 33..95 素材联合聚类(%d 色)\n", nasset);
    fprintf(f, "// 源图:");
    for(int i = 0; i < nimg; i++)
        fprintf(f, " %s", basename_noext(argv[3 + i], name, sizeof(name)));
    fprintf(f, "\n#include <stdint.h>\n\n");
    fprintf(f, "static const uint32_t c8pal_baked[%d] = {\n", BAKED_COUNT);
    for(int i = 0; i < BAKED_COUNT; i += 4){
        fprintf(f, "    0x%08X, 0x%08X, 0x%08X, 0x%08X,\n",
                g_pal[i], g_pal[i + 1], g_pal[i + 2], g_pal[i + 3]);
    }
    fprintf(f, "};\n");
    fclose(f);
    printf("wrote %s\n", out_hdr);
    return 0;
}
