#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "render/c8pal.h"
#include "render/c8pal_baked.h"
#include "utils/log.h"

// ---------------------------------------------------------------------------
// shadow 表与反查 LUT
// ---------------------------------------------------------------------------

static drm_warpper_t* s_dw;
static uint32_t s_shadow[256];

// 反查 LUT,0xFF = 未填充 sentinel(调色板 255 号永不分配,置 debug 品红)。
// opaque: key=RGB555,用于 a>=240(绝大多数像素:图片/文字主体/填充)。
// translucent: key=A4R4G4B4(半透明只有 ramp/AA,RGB 4bit 足够)。
static uint8_t s_lut_op[32 * 32 * 32];
static uint8_t s_lut_tr[16 * 16 * 16 * 16];

// 加权距离:半透明混合里 alpha 错档比色相错档更显眼
static inline uint32_t pal_dist(uint32_t c, int r, int g, int b, int a)
{
    int dr = (int)((c >> 16) & 0xFF) - r;
    int dg = (int)((c >> 8) & 0xFF) - g;
    int db = (int)(c & 0xFF) - b;
    int da = (int)(c >> 24) - a;
    return (uint32_t)(dr * dr + dg * dg + db * db + 4 * da * da);
}

static uint8_t scan_nearest(int r, int g, int b, int a)
{
    uint32_t best_d = UINT32_MAX;
    int best = C8PAL_IDX_TRANSPARENT;
    for(int i = 0; i < 255; i++){ // 255 号 sentinel 不参与
        uint32_t d = pal_dist(s_shadow[i], r, g, b, a);
        if(d < best_d){
            best_d = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

uint32_t c8pal_color(uint8_t idx)
{
    return s_shadow[idx];
}

uint8_t c8pal_index(uint32_t argb)
{
    uint32_t a = argb >> 24;
    if(a < 8)
        return C8PAL_IDX_TRANSPARENT;

    if(a >= 240){
        uint32_t key = ((argb >> 9) & 0x7C00) | ((argb >> 6) & 0x3E0) | ((argb >> 3) & 0x1F);
        uint8_t v = s_lut_op[key];
        if(v == 0xFF)
            v = s_lut_op[key] = scan_nearest((int)((argb >> 16) & 0xFF),
                                             (int)((argb >> 8) & 0xFF),
                                             (int)(argb & 0xFF), 255);
        return v;
    }

    uint32_t key = ((a >> 4) << 12) | (((argb >> 20) & 0xF) << 8)
                 | (((argb >> 12) & 0xF) << 4) | ((argb >> 4) & 0xF);
    uint8_t v = s_lut_tr[key];
    if(v == 0xFF){
        // 桶中心作代表色(低 4bit 补 8)
        v = s_lut_tr[key] = scan_nearest((int)(((argb >> 16) & 0xF0) | 8),
                                         (int)(((argb >> 8) & 0xF0) | 8),
                                         (int)((argb & 0xF0) | 8),
                                         (int)((a & 0xF0) | 8));
    }
    return v;
}

int c8pal_find_exact(uint32_t argb)
{
    for(int i = 0; i < 255; i++){
        if(s_shadow[i] == argb) return i;
    }
    return -1;
}

void c8pal_restore_baked(void)
{
    memcpy(s_shadow, c8pal_baked, C8PAL_BAKED_COUNT * sizeof(uint32_t));
}

void c8pal_write_range(int base, const uint32_t* colors, int n)
{
    if(base < 0 || base + n > 255){
        log_error("c8pal_write_range out of range: %d+%d", base, n);
        return;
    }
    memcpy(&s_shadow[base], colors, (size_t)n * sizeof(uint32_t));
}

void c8pal_commit(void)
{
    drm_warpper_set_palette(s_dw, s_shadow);
    memset(s_lut_op, 0xFF, sizeof(s_lut_op));
    memset(s_lut_tr, 0xFF, sizeof(s_lut_tr));
}

void c8pal_init(drm_warpper_t* dw)
{
    s_dw = dw;
    memset(s_shadow, 0, sizeof(s_shadow));
    c8pal_restore_baked();
#ifdef NDEBUG
    s_shadow[255] = 0xFF000000; // sentinel 项:真被画到就是 bug,release 给黑
#else
    s_shadow[255] = 0xFFFF00FF; // debug 品红,肉眼可见
#endif
    c8pal_commit();
}

// ---------------------------------------------------------------------------
// 颜色池
// ---------------------------------------------------------------------------

void c8pal_pool_add(uint32_t* pool, int* n, int cap, const uint32_t* colors, int cnt)
{
    for(int i = 0; i < cnt; i++){
        uint32_t c = colors[i];
        int dup = 0;
        for(int j = 0; j < *n; j++){
            if(pool[j] == c){
                dup = 1;
                break;
            }
        }
        if(dup) continue;
        if(*n >= cap){
            log_warn("c8pal pool full (cap %d), dropping colors", cap);
            return;
        }
        pool[(*n)++] = c;
    }
}

void c8pal_pool_add_ramp(uint32_t* pool, int* n, int cap, uint32_t color, int levels)
{
    uint32_t rgb = color & 0x00FFFFFF;
    uint32_t base_a = color >> 24;
    c8pal_pool_add(pool, n, cap, &color, 1);
    for(int i = 1; i <= levels; i++){
        uint32_t a = base_a * (uint32_t)(levels - i) / (uint32_t)levels;
        if(a == 0) continue; // 全透明有全局 idx 0
        uint32_t c = (a << 24) | rgb;
        c8pal_pool_add(pool, n, cap, &c, 1);
    }
}

// ---------------------------------------------------------------------------
// median-cut 量化(RGBA 4 维,alpha 权重 x2,确定性)
// ---------------------------------------------------------------------------

#define QUANT_MAX_SAMPLES 32768

typedef struct {
    int start, len;
} qbox_t;

static int s_sort_ch; // 0=r 1=g 2=b 3=a

static int sample_cmp(const void* pa, const void* pb)
{
    const uint8_t* a = (const uint8_t*)pa;
    const uint8_t* b = (const uint8_t*)pb;
    if(a[s_sort_ch] != b[s_sort_ch])
        return (int)a[s_sort_ch] - (int)b[s_sort_ch];
    // 全像素值 tie-break,消除 qsort 不稳定带来的平台差异
    return memcmp(a, b, 4);
}

// samples: rgba 字节序 x count。返回聚出的色数(<= max_n)
static int median_cut(uint8_t* samples, int count, int max_n, uint32_t* out_pal)
{
    qbox_t boxes[256];
    int nbox = 1;
    boxes[0].start = 0;
    boxes[0].len = count;

    while(nbox < max_n){
        // 选加权极差最大的箱切(alpha 极差 x2)
        int pick = -1, pick_ch = 0;
        int pick_score = 0;
        for(int i = 0; i < nbox; i++){
            if(boxes[i].len < 2) continue;
            int lo[4] = {255, 255, 255, 255}, hi[4] = {0, 0, 0, 0};
            const uint8_t* p = samples + (size_t)boxes[i].start * 4;
            for(int j = 0; j < boxes[i].len; j++){
                for(int c = 0; c < 4; c++){
                    int v = p[j * 4 + c];
                    if(v < lo[c]) lo[c] = v;
                    if(v > hi[c]) hi[c] = v;
                }
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
        if(pick < 0) break; // 所有箱都不可再切(色数已穷尽)

        qbox_t* bx = &boxes[pick];
        s_sort_ch = pick_ch;
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

static int quantize_palette(const uint32_t* px, int w, int h, int max_n, uint32_t* out_pal)
{
    size_t total = (size_t)w * h;
    int stride = (int)(total / QUANT_MAX_SAMPLES) + 1;

    uint8_t* samples = malloc(QUANT_MAX_SAMPLES * 4 + 4);
    if(!samples) return -1;

    int count = 0;
    for(size_t i = 0; i < total; i += (size_t)stride){
        uint32_t c = px[i];
        if((c >> 24) < 8) continue; // 透明像素不参与(落全局透明项)
        samples[count * 4 + 0] = (uint8_t)(c >> 16);
        samples[count * 4 + 1] = (uint8_t)(c >> 8);
        samples[count * 4 + 2] = (uint8_t)c;
        samples[count * 4 + 3] = (uint8_t)(c >> 24);
        count++;
    }
    if(count == 0){
        free(samples);
        return 0;
    }

    int n = median_cut(samples, count, max_n, out_pal);
    free(samples);
    return n;
}

// ---------------------------------------------------------------------------
// Floyd-Steinberg 抖动出索引图,并把 px 就地改写为量化展开
// idx 图:0xFF = 透明(不占调色板项);其余为 pal 内相对索引
// ---------------------------------------------------------------------------

// 抖动期间的局部反查缓存:A4R4G4B4(误差反馈会补偿 4bit key 的取整噪声)
static uint8_t dither_lookup(uint8_t* cache, const uint32_t* pal, int n,
                             int r, int g, int b, int a)
{
    uint32_t key = ((uint32_t)(a >> 4) << 12) | ((uint32_t)(r >> 4) << 8)
                 | ((uint32_t)(g >> 4) << 4) | (uint32_t)(b >> 4);
    uint8_t v = cache[key];
    if(v != 0xFF) return v;

    uint32_t best_d = UINT32_MAX;
    int best = 0;
    for(int i = 0; i < n; i++){
        uint32_t d = pal_dist(pal[i], r, g, b, a);
        if(d < best_d){
            best_d = d;
            best = i;
        }
    }
    cache[key] = (uint8_t)best;
    return (uint8_t)best;
}

static inline int clamp255(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// 成功返回 0,idx 由调用方提供(w*h 字节)
static int fs_dither(uint32_t* px, int w, int h, const uint32_t* pal, int n, uint8_t* idx)
{
    // 两行 RGB 误差(int16 x3),透明像素不吃不吐误差
    int16_t* err = calloc((size_t)(w + 2) * 2 * 3, sizeof(int16_t));
    uint8_t* cache = malloc(16 * 16 * 16 * 16);
    if(!err || !cache){
        free(err);
        free(cache);
        return -1;
    }
    memset(cache, 0xFF, 16 * 16 * 16 * 16);

    int16_t* cur = err + 3;            // [-1..w] 有效,偏移 1 防越界
    int16_t* nxt = err + (w + 2) * 3 + 3;

    for(int y = 0; y < h; y++){
        memset(nxt - 3, 0, (size_t)(w + 2) * 3 * sizeof(int16_t));
        for(int x = 0; x < w; x++){
            uint32_t c = px[(size_t)y * w + x];
            int a = (int)(c >> 24);
            if(a < 8){
                idx[(size_t)y * w + x] = 0xFF;
                px[(size_t)y * w + x] = 0;
                continue;
            }
            int r = clamp255((int)((c >> 16) & 0xFF) + cur[x * 3 + 0] / 16);
            int g = clamp255((int)((c >> 8) & 0xFF) + cur[x * 3 + 1] / 16);
            int b = clamp255((int)(c & 0xFF) + cur[x * 3 + 2] / 16);

            uint8_t i = dither_lookup(cache, pal, n, r, g, b, a);
            idx[(size_t)y * w + x] = i;
            px[(size_t)y * w + x] = pal[i];

            int er = r - (int)((pal[i] >> 16) & 0xFF);
            int eg = g - (int)((pal[i] >> 8) & 0xFF);
            int eb = b - (int)(pal[i] & 0xFF);
            // 标准 F-S 权重: 右7/16 左下3/16 下5/16 右下1/16(err 存 16 倍)
            cur[(x + 1) * 3 + 0] += (int16_t)(er * 7);
            cur[(x + 1) * 3 + 1] += (int16_t)(eg * 7);
            cur[(x + 1) * 3 + 2] += (int16_t)(eb * 7);
            nxt[(x - 1) * 3 + 0] += (int16_t)(er * 3);
            nxt[(x - 1) * 3 + 1] += (int16_t)(eg * 3);
            nxt[(x - 1) * 3 + 2] += (int16_t)(eb * 3);
            nxt[x * 3 + 0] += (int16_t)(er * 5);
            nxt[x * 3 + 1] += (int16_t)(eg * 5);
            nxt[x * 3 + 2] += (int16_t)(eb * 5);
            nxt[(x + 1) * 3 + 0] += (int16_t)er;
            nxt[(x + 1) * 3 + 1] += (int16_t)eg;
            nxt[(x + 1) * 3 + 2] += (int16_t)eb;
        }
        int16_t* t = cur;
        cur = nxt;
        nxt = t;
    }

    free(err);
    free(cache);
    return 0;
}

// ---------------------------------------------------------------------------
// 磁盘缓存 (<img>.c8pal / <img>.c8i,源图同目录)
// ---------------------------------------------------------------------------

typedef struct {
    char magic[4];
    uint8_t version;
    uint8_t count;
    uint16_t quota;
    uint32_t src_size;
    uint32_t src_mtime;
} __attribute__((packed)) c8pal_file_hdr_t;

typedef struct {
    char magic[4];
    uint8_t version;
    uint8_t count;
    uint16_t w;
    uint16_t h;
    uint32_t src_size;
    uint32_t src_mtime;
} __attribute__((packed)) c8i_file_hdr_t;

static int src_stat(const char* path, uint32_t* size, uint32_t* mtime)
{
    struct stat st;
    if(stat(path, &st) != 0) return -1;
    *size = (uint32_t)st.st_size;
    *mtime = (uint32_t)st.st_mtime;
    return 0;
}

// 配额编进文件名:同一张图可能被不同 owner 以不同配额量化
// (如 transition 图 32 与 arknights class 16),共用一个文件会互相覆盖永远 miss
static void cache_path(char* out, size_t cap, const char* img, const char* ext, int quota)
{
    snprintf(out, cap, "%s.q%d%s", img, quota, ext);
}

// 命中返回色数并填 out_pal + 展开 px,未命中/校验失败返回 <0
static int cache_load(const char* img_path, uint32_t* px, int w, int h,
                      int max_n, uint32_t* out_pal)
{
    uint32_t ssize, smtime;
    if(src_stat(img_path, &ssize, &smtime) != 0) return -1;

    char path[256];
    cache_path(path, sizeof(path), img_path, ".c8pal", max_n);
    FILE* f = fopen(path, "rb");
    if(!f) return -1;

    c8pal_file_hdr_t ph;
    int ok = fread(&ph, sizeof(ph), 1, f) == 1
          && memcmp(ph.magic, "C8P1", 4) == 0
          && ph.version == C8PAL_CACHE_VERSION
          && ph.quota == (uint16_t)max_n
          && ph.src_size == ssize && ph.src_mtime == smtime
          && ph.count > 0 && ph.count <= max_n
          && fread(out_pal, sizeof(uint32_t), ph.count, f) == ph.count;
    fclose(f);
    if(!ok) return -1;

    cache_path(path, sizeof(path), img_path, ".c8i", max_n);
    f = fopen(path, "rb");
    if(!f) return -1;

    c8i_file_hdr_t ih;
    uint8_t* idx = malloc((size_t)w * h);
    ok = idx != NULL
      && fread(&ih, sizeof(ih), 1, f) == 1
      && memcmp(ih.magic, "C8I1", 4) == 0
      && ih.version == C8PAL_CACHE_VERSION
      && ih.count == ph.count
      && ih.w == (uint16_t)w && ih.h == (uint16_t)h
      && ih.src_size == ssize && ih.src_mtime == smtime
      && fread(idx, 1, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    if(!ok){
        free(idx);
        return -1;
    }

    for(size_t i = 0; i < (size_t)w * h; i++)
        px[i] = (idx[i] == 0xFF) ? 0
              : (idx[i] < ph.count ? out_pal[idx[i]] : 0);
    free(idx);
    return ph.count;
}

static void cache_store(const char* img_path, const uint32_t* pal, int n,
                        const uint8_t* idx, int w, int h, int max_n)
{
    uint32_t ssize, smtime;
    if(src_stat(img_path, &ssize, &smtime) != 0) return;

    char path[256];
    cache_path(path, sizeof(path), img_path, ".c8pal", max_n);
    FILE* f = fopen(path, "wb");
    if(!f) return; // 只读介质(res/ 等),静默降级为每次现算

    c8pal_file_hdr_t ph = {
        .magic = {'C', '8', 'P', '1'},
        .version = C8PAL_CACHE_VERSION,
        .count = (uint8_t)n,
        .quota = (uint16_t)max_n,
        .src_size = ssize,
        .src_mtime = smtime,
    };
    int ok = fwrite(&ph, sizeof(ph), 1, f) == 1
          && fwrite(pal, sizeof(uint32_t), n, f) == (size_t)n;
    fclose(f);
    if(!ok){
        remove(path);
        return;
    }

    cache_path(path, sizeof(path), img_path, ".c8i", max_n);
    f = fopen(path, "wb");
    if(!f) return;

    c8i_file_hdr_t ih = {
        .magic = {'C', '8', 'I', '1'},
        .version = C8PAL_CACHE_VERSION,
        .count = (uint8_t)n,
        .w = (uint16_t)w,
        .h = (uint16_t)h,
        .src_size = ssize,
        .src_mtime = smtime,
    };
    ok = fwrite(&ih, sizeof(ih), 1, f) == 1
      && fwrite(idx, 1, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    if(!ok)
        remove(path);
}

int c8pal_load_or_quantize(const char* img_path, uint32_t* px, int w, int h,
                           int max_n, uint32_t* out_pal)
{
    if(!px || w <= 0 || h <= 0 || max_n < 2 || max_n > 254) return -1;

    if(img_path){
        int n = cache_load(img_path, px, w, h, max_n, out_pal);
        if(n >= 0){
            log_info("c8pal: cache hit %s (%d colors)", img_path, n);
            return n;
        }
    }

    int n = quantize_palette(px, w, h, max_n, out_pal);
    if(n < 0) return -1;
    if(n == 0) return 0; // 全透明图:无需调色板项

    uint8_t* idx = malloc((size_t)w * h);
    if(!idx) return -1;
    if(fs_dither(px, w, h, out_pal, n, idx) != 0){
        free(idx);
        return -1;
    }

    if(img_path)
        cache_store(img_path, out_pal, n, idx, w, h, max_n);
    free(idx);
    log_info("c8pal: quantized %s -> %d colors", img_path ? img_path : "(anon)", n);
    return n;
}
