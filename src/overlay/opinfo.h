#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "overlay/overlay.h"
#include "utils/cacheassets.h"

typedef enum {
    OPINFO_TYPE_IMAGE,
    OPINFO_TYPE_ARKNIGHTS,
    OPINFO_TYPE_CUSTOM,
    OPINFO_TYPE_NONE,
} opinfo_type_t;

// ============================================================================
// 元素引擎 —— image / custom 类型的绘制后端
//
// 两种 overlay.type 在解析阶段被翻译成一张"元素列表"，由同一个 worker 状态机驱动：
//   - image  -> 1 个静态 image 元素
//   - custom -> epconfig.json 里 overlay.options.elements 的直接映射
// arknights 类型不走引擎，用 overlay_opinfo_show_arknights() 的专用实现。
// ============================================================================

typedef enum {
    OPINFO_EL_TEXT,        // 水平文本（支持 \n 多行）
    OPINFO_EL_TEXT_ROT90,  // 顺时针旋转 90° 的文本
    OPINFO_EL_IMAGE,       // 图片：用户图(image_path) 或 cacheasset(cacheasset_id)
    OPINFO_EL_RECT,        // 纯色矩形
    OPINFO_EL_BARCODE,     // code128 条形码（旋转 90°，带文字）
    OPINFO_EL_CORNER_FADE, // 右下角渐变三角（程序化绘制，位置固定右下）
} opinfo_element_type_t;

typedef enum {
    OPINFO_ANIM_NONE,       // start_frame 时一次性画出
    OPINFO_ANIM_TYPEWRITER, // text: 逐 codepoint 打字机
    OPINFO_ANIM_EINK,       // image/rect/barcode: 黑白闪烁数次后出内容
    OPINFO_ANIM_FADE,       // image/text/rect: 不透明度 0->255 淡入
    OPINFO_ANIM_WIPE,       // rect/image: 按 cubic-bezier 从 0 划入（direction 定方向）
    OPINFO_ANIM_SCROLL,     // image: 垂直循环滚动，永不结束（除非 end_frame）
    OPINFO_ANIM_GROW,       // corner_fade: 半径从 0 长到目标值
    OPINFO_ANIM_MOVE,       // text/text_rot90/image/rect/barcode: 从 from_dx/from_dy 偏移滑入落点
    OPINFO_ANIM_SCRAMBLE,   // text: 乱码解码（随机字符跳变、逐个稳定成真实文本）
    OPINFO_ANIM_BLINK,      // text/text_rot90/image/rect/barcode: 周期闪烁，永不结束（除非 end_frame）
    OPINFO_ANIM_SPRITE,     // image: 横向精灵图逐帧循环播放，永不结束（除非 end_frame）
    OPINFO_ANIM_SWAY,       // text/text_rot90/image/rect/barcode: 沿 from_dx/from_dy 正弦晃动，永不结束（除非 end_frame）
} opinfo_anim_t;

// wipe 划入方向
typedef enum {
    OPINFO_WIPE_LTR, // 从左往右（默认）
    OPINFO_WIPE_RTL, // 从右往左
    OPINFO_WIPE_TTB, // 从上往下
    OPINFO_WIPE_BTT, // 从下往上
} opinfo_wipe_dir_t;

// x,y 从哪个角落量起（右/下锚定时 x,y 是元素相应边到屏幕边缘的距离）。
// 图片按原生尺寸绘制，锚定让"贴右下角"不依赖图片尺寸。
typedef enum {
    OPINFO_ANCHOR_TL,
    OPINFO_ANCHOR_TR,
    OPINFO_ANCHOR_BL,
    OPINFO_ANCHOR_BR,
} opinfo_anchor_t;

typedef struct {
    opinfo_element_type_t type;
    opinfo_anim_t anim;
    opinfo_anchor_t anchor;

    // 全部为 360x640 基准的逻辑单位，绘制时套 S()。
    // w/h: text 缺省(0)=宽到屏幕边缘、高按行数x行高；image 忽略（用图片原生尺寸）；
    //      corner_fade 的 w = 目标半径。
    int x, y;
    int w, h;

    int start_frame; // 动画从第几帧开始（30fps）
    int end_frame;   // >0: 到该帧隐藏元素并视为播放完毕；0=永不退场
    // 语义随 anim 变化：
    //   typewriter=每 codepoint 帧数  eink=每闪烁态帧数  fade=每帧不透明度增量
    //   wipe=划入总帧数              scroll=每帧滚动像素  grow=每帧半径增量
    //   move=滑入总帧数              scramble=每 codepoint 帧数  blink=半周期帧数
    //   sprite=每帧停留帧数          sway=一个来回周期帧数
    int speed;
    int wipe_dir;         // opinfo_wipe_dir_t，仅 wipe 用
    int from_dx, from_dy; // move: 起点相对落点的偏移（360 基准）；sway: 摆幅（360 基准）
    int frames;           // sprite: 横向精灵图的帧数（单帧宽 = 图宽/frames）

    // text / text_rot90 / barcode
    char text[256];
    int font_role;    // font_role_t (body/title/display/icon)
    int font_size;    // 360 基准字号
    int line_height;  // 360 基准行高，0=字体默认
    int letter_space; // 360 基准字距
    bool faux_bold;   // text_rot90: 整体 +1px 重画一遍加粗
    bool bold_split;  // text_rot90: 空格前 faux bold、空格后常规（无空格则整体加粗）

    uint32_t color;   // text/rect/corner_fade 颜色 (ARGB)
    int border_width; // rect: >0 画空心边框（360 基准线宽），0 实心

    // image 来源二选一
    int cacheasset_id;    // >=0: cacheasset_asset_id_t，忽略 image_path
    char image_path[128]; // 用户图片绝对路径（overlay_opinfo_load_image 加载）
    int image_w, image_h; // 加载后的物理像素尺寸
    uint32_t* image_addr;
} olopinfo_element_t;

typedef struct {
    opinfo_type_t type;

    // 通用参数
    int appear_time;
    // 进场滑入动画时长(us)，<=0 用默认 1s
    int duration;
    // 用户图片加载后的最近邻放大倍数(720p 档显示 360 基准旧素材时为 UI_SCALE，否则 1)
    int src_upscale;

    // image 类型：图片路径（build 后转入元素）
    char image_path[128];

    // arknights 带有简单动态效果的明日方舟通行证模板（专用实现，不走元素引擎）
    char operator_name[20];
    char operator_code[40];
    char barcode_text[40];
    char staff_text[40];
    char aux_text[256];

    char class_path[128];
    int class_w;
    int class_h;
    uint32_t* class_addr;

    char logo_path[128];
    int logo_w;
    int logo_h;
    uint32_t* logo_addr;

    char rhodes_text[40];        // 非空时替代默认 rhodes logo 图片
    char top_right_bar_text[40]; // 非空时覆盖 top_right_bar 内嵌文字
    uint32_t color;

    // 统一元素列表：image / custom 解析后落到这里（堆分配、精确大小，归 operator entry 所有，
    // 用 overlay_opinfo_free_elements 释放）；arknights 类型不用
    int element_count;
    olopinfo_element_t* elements;

    // C8: load 阶段攒好的动态段颜色池(theme ramp/元素色/用户图量化结果)。
    // show 入口在层离屏时整段写表+commit;落盘走 LUT 反查只认颜色不认位置,
    // 池内顺序无所谓。image 模式独占 1..254,其余写 96..254。
    uint32_t c8_pool[C8PAL_IMAGE_QUOTA];
    int c8_pool_n;

} olopinfo_params_t;

// 元素字段默认值（cacheasset_id=-1、白色、body 14 等），解析/builder 共用
void overlay_opinfo_element_init(olopinfo_element_t* el);

// 把 image 类型的 options 翻译成元素列表（分配 params->elements）。
// 成功返回 0；失败返回 -1（此时 elements 为 NULL）。
int overlay_opinfo_build_image_elements(olopinfo_params_t* params);

// 加载/释放用户图片的像素数据：arknights 走 class/logo 字段，
// 其余类型遍历元素列表（cacheasset 来源不经过这里）
void overlay_opinfo_load_image(olopinfo_params_t* params);
void overlay_opinfo_free_image(olopinfo_params_t* params);

// 释放整张元素列表（先 free 图片再 free 数组）
void overlay_opinfo_free_elements(olopinfo_params_t* params);

// image / custom 显示入口：驱动 params->elements
void overlay_opinfo_show_elements(overlay_t* overlay, olopinfo_params_t* params);

// arknights 显示入口：专用实现
void overlay_opinfo_show_arknights(overlay_t* overlay, olopinfo_params_t* params);
