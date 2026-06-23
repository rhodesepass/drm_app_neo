/*******************************************************************************
 * Size: 10 px
 * Bpp: 4
 * Opts: --bpp 4 --size 10 --no-compress --font ../../../live_desktop/电子通行证/通行证分享用/字体/Bebas Neue Bold.otf --range 32-127 --format lvgl
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifndef UI_FONT_BEBAS_BOLD_10
#define UI_FONT_BEBAS_BOLD_10 1
#endif

#if UI_FONT_BEBAS_BOLD_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x97, 0x97, 0x97, 0x87, 0x86, 0x32, 0x97,

    /* U+0022 "\"" */
    0x89, 0xc7, 0x6b, 0x0, 0x0,

    /* U+0023 "#" */
    0xe, 0x59, 0xe, 0x67, 0x6f, 0xec, 0x3b, 0xa4,
    0xae, 0xf8, 0x67, 0xd0, 0x86, 0xe0,

    /* U+0024 "$" */
    0x1, 0x0, 0x1b, 0xb0, 0x7b, 0xb7, 0x7b, 0x11,
    0xc, 0xb0, 0x32, 0xc6, 0x88, 0xa8, 0x2d, 0xc1,
    0x2, 0x20,

    /* U+0025 "%" */
    0x5b, 0x31, 0x90, 0xa4, 0x76, 0x30, 0xa4, 0x7a,
    0x97, 0xa4, 0xa9, 0x8b, 0x4a, 0xb4, 0x8b, 0x0,
    0x92, 0x8b, 0x6, 0x30, 0xa9,

    /* U+0026 "&" */
    0x2c, 0xf4, 0x7, 0x90, 0x0, 0x7a, 0x4a, 0x2,
    0xff, 0xf0, 0x8a, 0x5b, 0x8, 0x96, 0xb0, 0x3e,
    0xcc, 0x0,

    /* U+0027 "'" */
    0x96, 0x84, 0x0,

    /* U+0028 "(" */
    0x2d, 0x47, 0x90, 0x88, 0x8, 0x80, 0x88, 0x7,
    0x90, 0x2d, 0x40,

    /* U+0029 ")" */
    0xc9, 0x1, 0xf0, 0xf, 0x0, 0xf0, 0xf, 0x1,
    0xf0, 0xc9, 0x0,

    /* U+002A "*" */
    0x6, 0x50, 0xaa, 0xa9, 0x19, 0x90, 0x23, 0x42,

    /* U+002B "+" */
    0x3, 0x20, 0x6, 0x50, 0x8d, 0xd8, 0x6, 0x50,

    /* U+002C "," */
    0x0, 0x97, 0x53,

    /* U+002D "-" */
    0xaf, 0x50,

    /* U+002E "." */
    0x0, 0x97,

    /* U+002F "/" */
    0x0, 0x69, 0x0, 0xc2, 0x2, 0xc0, 0x9, 0x60,
    0xe, 0x0, 0x59, 0x0, 0xc3, 0x0,

    /* U+0030 "0" */
    0x3d, 0xc2, 0x97, 0x89, 0xa6, 0x7a, 0xa6, 0x7a,
    0xa6, 0x7a, 0x97, 0x89, 0x3d, 0xd2,

    /* U+0031 "1" */
    0x1e, 0xc, 0xf0, 0xf, 0x0, 0xf0, 0xf, 0x0,
    0xf0, 0xf, 0x0,

    /* U+0032 "2" */
    0x2c, 0xd3, 0x88, 0x89, 0x32, 0x97, 0x2, 0xe2,
    0x1d, 0x50, 0x6a, 0x0, 0x9f, 0xf9,

    /* U+0033 "3" */
    0x3d, 0xc2, 0x97, 0x98, 0x10, 0x98, 0x8, 0xf2,
    0x21, 0x98, 0x97, 0x98, 0x3d, 0xd2,

    /* U+0034 "4" */
    0x1, 0xf5, 0x7, 0xf5, 0xd, 0xe5, 0x4c, 0xb5,
    0xa5, 0xb5, 0xdf, 0xfd, 0x0, 0xb5,

    /* U+0035 "5" */
    0x7f, 0xf7, 0x88, 0x0, 0x9c, 0xd3, 0x97, 0x99,
    0x32, 0x79, 0x87, 0x98, 0x3d, 0xc2,

    /* U+0036 "6" */
    0x3c, 0xd3, 0x98, 0x78, 0xad, 0xe4, 0xa8, 0x89,
    0xa6, 0x6a, 0x98, 0x88, 0x3d, 0xc2,

    /* U+0037 "7" */
    0xaf, 0xf9, 0x0, 0x97, 0x0, 0xe3, 0x2, 0xe0,
    0x6, 0xa0, 0xb, 0x60, 0xf, 0x20,

    /* U+0038 "8" */
    0x3d, 0xc3, 0xa7, 0x89, 0xa7, 0x89, 0x4f, 0xf4,
    0xa7, 0x8a, 0xa7, 0x8a, 0x3d, 0xd3,

    /* U+0039 "9" */
    0x3d, 0xc2, 0x97, 0x98, 0xb6, 0x79, 0xa8, 0x99,
    0x4e, 0xd9, 0x86, 0x88, 0x3d, 0xc2,

    /* U+003A ":" */
    0x86, 0x10, 0x0, 0x0, 0x97,

    /* U+003B ";" */
    0x86, 0x10, 0x0, 0x0, 0x97, 0x53,

    /* U+003C "<" */
    0x0, 0x0, 0x4, 0xa6, 0xad, 0x30, 0x5, 0xb6,
    0x0, 0x0,

    /* U+003D "=" */
    0x7b, 0xb7, 0x0, 0x0, 0x8c, 0xc7,

    /* U+003E ">" */
    0x0, 0x0, 0x7a, 0x40, 0x3, 0xd9, 0x7b, 0x50,
    0x0, 0x0,

    /* U+003F "?" */
    0x4d, 0xb0, 0xb5, 0xb5, 0x51, 0xd4, 0x4, 0xd0,
    0xb, 0x40, 0x6, 0x10, 0xd, 0x30,

    /* U+0040 "@" */
    0x0, 0x9c, 0xc7, 0x0, 0xc5, 0x0, 0xa6, 0x69,
    0x6d, 0xd6, 0xaa, 0x4d, 0x2f, 0x39, 0xb3, 0xf2,
    0xe6, 0x6a, 0x5a, 0xad, 0xa0, 0x3d, 0x20, 0x14,
    0x0, 0x4b, 0xdb, 0x30,

    /* U+0041 "A" */
    0xd, 0xe0, 0xf, 0xf1, 0x2d, 0xd3, 0x5a, 0xb6,
    0x78, 0x98, 0xaf, 0xfb, 0xc3, 0x3d,

    /* U+0042 "B" */
    0x9f, 0xd3, 0x97, 0x89, 0x98, 0x89, 0x9f, 0xf4,
    0x98, 0x6b, 0x97, 0x6b, 0x9f, 0xd4,

    /* U+0043 "C" */
    0x3d, 0xc2, 0x97, 0x88, 0xa6, 0x23, 0xa6, 0x0,
    0xa6, 0x45, 0x97, 0x88, 0x3d, 0xc2,

    /* U+0044 "D" */
    0x9f, 0xd4, 0x97, 0x6b, 0x97, 0x4c, 0x97, 0x4c,
    0x97, 0x4c, 0x97, 0x6b, 0x9f, 0xd4,

    /* U+0045 "E" */
    0x9f, 0xf6, 0x97, 0x0, 0x97, 0x0, 0x9f, 0xf0,
    0x97, 0x0, 0x97, 0x0, 0x9f, 0xf6,

    /* U+0046 "F" */
    0x9f, 0xf4, 0x97, 0x0, 0x97, 0x0, 0x9f, 0xe0,
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0,

    /* U+0047 "G" */
    0x3d, 0xc2, 0x97, 0x88, 0xa6, 0x23, 0xa6, 0xe9,
    0xa6, 0x79, 0x97, 0x88, 0x3d, 0xc2,

    /* U+0048 "H" */
    0x97, 0x3d, 0x97, 0x3d, 0x97, 0x3d, 0x9f, 0xfd,
    0x97, 0x3d, 0x97, 0x3d, 0x97, 0x3d,

    /* U+0049 "I" */
    0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,

    /* U+004A "J" */
    0xe, 0x30, 0xe3, 0xe, 0x30, 0xe3, 0xe, 0x30,
    0xf2, 0xea, 0x0,

    /* U+004B "K" */
    0x97, 0x4c, 0x9, 0x8c, 0x40, 0x9b, 0xd0, 0x9,
    0xfa, 0x0, 0x9b, 0xf1, 0x9, 0x7a, 0x70, 0x97,
    0x4d, 0x0,

    /* U+004C "L" */
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0, 0x97, 0x0,
    0x97, 0x0, 0x97, 0x0, 0x9f, 0xf4,

    /* U+004D "M" */
    0x9f, 0x9, 0xf0, 0x9f, 0x2b, 0xf0, 0x9e, 0x5d,
    0xf0, 0x9c, 0x8c, 0xf0, 0x99, 0xd9, 0xf0, 0x96,
    0xf7, 0xf0, 0x95, 0xe4, 0xf0,

    /* U+004E "N" */
    0x9e, 0x1d, 0x9f, 0x4d, 0x9d, 0x8d, 0x99, 0xcd,
    0x96, 0xed, 0x95, 0xad, 0x95, 0x6d,

    /* U+004F "O" */
    0x3d, 0xc2, 0x97, 0x89, 0xa6, 0x7a, 0xa6, 0x7a,
    0xa6, 0x7a, 0x97, 0x89, 0x3d, 0xd2,

    /* U+0050 "P" */
    0x9f, 0xd3, 0x97, 0x89, 0x97, 0x89, 0x9f, 0xd3,
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0,

    /* U+0051 "Q" */
    0x3d, 0xd3, 0x97, 0x89, 0xa6, 0x7a, 0xa6, 0x7a,
    0xa6, 0x7a, 0x97, 0x88, 0x3d, 0xec,

    /* U+0052 "R" */
    0x9f, 0xd4, 0x97, 0x7a, 0x98, 0x89, 0x9f, 0xf3,
    0x98, 0x89, 0x97, 0x6b, 0x97, 0x6c,

    /* U+0053 "S" */
    0x4d, 0xc1, 0xa7, 0x96, 0x8b, 0x0, 0xd, 0xa0,
    0x21, 0xe4, 0xa5, 0xb6, 0x4d, 0xc1,

    /* U+0054 "T" */
    0xef, 0xf7, 0xc, 0x50, 0xc, 0x50, 0xc, 0x50,
    0xc, 0x50, 0xc, 0x50, 0xc, 0x50,

    /* U+0055 "U" */
    0xa7, 0x69, 0xa7, 0x69, 0xa7, 0x69, 0xa7, 0x69,
    0xa7, 0x69, 0x98, 0x88, 0x3d, 0xc2,

    /* U+0056 "V" */
    0xd4, 0x2d, 0xa6, 0x5a, 0x89, 0x78, 0x5b, 0x95,
    0x3e, 0xc3, 0xf, 0xe0, 0xe, 0xe0,

    /* U+0057 "W" */
    0xc4, 0xb7, 0x86, 0xa6, 0xc8, 0xa5, 0x97, 0xea,
    0xb3, 0x79, 0xeb, 0xd1, 0x5c, 0xbd, 0xe0, 0x3f,
    0x9e, 0xd0, 0x1f, 0x7d, 0xc0,

    /* U+0058 "X" */
    0x98, 0x1e, 0x4, 0xd7, 0x80, 0xe, 0xe3, 0x0,
    0xbf, 0x0, 0xe, 0xd4, 0x5, 0xa7, 0xa0, 0xa5,
    0x2f, 0x0,

    /* U+0059 "Y" */
    0xc5, 0x4c, 0x89, 0x87, 0x3d, 0xc2, 0xe, 0xd0,
    0xa, 0x90, 0x8, 0x80, 0x8, 0x80,

    /* U+005A "Z" */
    0xaf, 0xf7, 0x0, 0xe3, 0x5, 0xd0, 0xb, 0x60,
    0x2e, 0x0, 0x89, 0x0, 0xcf, 0xf7,

    /* U+005B "[" */
    0x8f, 0x48, 0x80, 0x88, 0x8, 0x80, 0x88, 0x8,
    0x80, 0x8f, 0x40,

    /* U+005C "\\" */
    0xc3, 0x0, 0x59, 0x0, 0xe, 0x0, 0x9, 0x60,
    0x2, 0xc0, 0x0, 0xc2, 0x0, 0x69,

    /* U+005D "]" */
    0xbf, 0x0, 0xf0, 0xf, 0x0, 0xf0, 0xf, 0x0,
    0xf0, 0xbf, 0x0,

    /* U+005E "^" */
    0xa, 0x90, 0x1b, 0xc1, 0x94, 0x58,

    /* U+005F "_" */
    0xcc, 0xcc, 0xc0,

    /* U+0060 "`" */
    0x2c, 0x0, 0x10,

    /* U+0061 "a" */
    0xd, 0xe0, 0xf, 0xf1, 0x2d, 0xd3, 0x5a, 0xb6,
    0x78, 0x98, 0xaf, 0xfb, 0xc3, 0x3d,

    /* U+0062 "b" */
    0x9f, 0xd3, 0x97, 0x89, 0x98, 0x89, 0x9f, 0xf4,
    0x98, 0x6b, 0x97, 0x6b, 0x9f, 0xd4,

    /* U+0063 "c" */
    0x3d, 0xc2, 0x97, 0x88, 0xa6, 0x23, 0xa6, 0x0,
    0xa6, 0x45, 0x97, 0x88, 0x3d, 0xc2,

    /* U+0064 "d" */
    0x9f, 0xd4, 0x97, 0x6b, 0x97, 0x4c, 0x97, 0x4c,
    0x97, 0x4c, 0x97, 0x6b, 0x9f, 0xd4,

    /* U+0065 "e" */
    0x9f, 0xf6, 0x97, 0x0, 0x97, 0x0, 0x9f, 0xf0,
    0x97, 0x0, 0x97, 0x0, 0x9f, 0xf6,

    /* U+0066 "f" */
    0x9f, 0xf4, 0x97, 0x0, 0x97, 0x0, 0x9f, 0xe0,
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0,

    /* U+0067 "g" */
    0x3d, 0xc2, 0x97, 0x88, 0xa6, 0x23, 0xa6, 0xe9,
    0xa6, 0x79, 0x97, 0x88, 0x3d, 0xc2,

    /* U+0068 "h" */
    0x97, 0x3d, 0x97, 0x3d, 0x97, 0x3d, 0x9f, 0xfd,
    0x97, 0x3d, 0x97, 0x3d, 0x97, 0x3d,

    /* U+0069 "i" */
    0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,

    /* U+006A "j" */
    0xe, 0x30, 0xe3, 0xe, 0x30, 0xe3, 0xe, 0x30,
    0xf2, 0xea, 0x0,

    /* U+006B "k" */
    0x97, 0x4c, 0x9, 0x8c, 0x40, 0x9b, 0xd0, 0x9,
    0xfa, 0x0, 0x9b, 0xf1, 0x9, 0x7a, 0x70, 0x97,
    0x4d, 0x0,

    /* U+006C "l" */
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0, 0x97, 0x0,
    0x97, 0x0, 0x97, 0x0, 0x9f, 0xf4,

    /* U+006D "m" */
    0x9f, 0x9, 0xf0, 0x9f, 0x2b, 0xf0, 0x9e, 0x5d,
    0xf0, 0x9c, 0x8c, 0xf0, 0x99, 0xd9, 0xf0, 0x96,
    0xf7, 0xf0, 0x95, 0xe4, 0xf0,

    /* U+006E "n" */
    0x9e, 0x1d, 0x9f, 0x4d, 0x9d, 0x8d, 0x99, 0xcd,
    0x96, 0xed, 0x95, 0xad, 0x95, 0x6d,

    /* U+006F "o" */
    0x3d, 0xc2, 0x97, 0x89, 0xa6, 0x7a, 0xa6, 0x7a,
    0xa6, 0x7a, 0x97, 0x89, 0x3d, 0xd2,

    /* U+0070 "p" */
    0x9f, 0xd3, 0x97, 0x89, 0x97, 0x89, 0x9f, 0xd3,
    0x97, 0x0, 0x97, 0x0, 0x97, 0x0,

    /* U+0071 "q" */
    0x3d, 0xd3, 0x97, 0x89, 0xa6, 0x7a, 0xa6, 0x7a,
    0xa6, 0x7a, 0x97, 0x88, 0x3d, 0xec,

    /* U+0072 "r" */
    0x9f, 0xd4, 0x97, 0x7a, 0x98, 0x89, 0x9f, 0xf3,
    0x98, 0x89, 0x97, 0x6b, 0x97, 0x6c,

    /* U+0073 "s" */
    0x4d, 0xc1, 0xa7, 0x96, 0x8b, 0x0, 0xd, 0xa0,
    0x21, 0xe4, 0xa5, 0xb6, 0x4d, 0xc1,

    /* U+0074 "t" */
    0xef, 0xf7, 0xc, 0x50, 0xc, 0x50, 0xc, 0x50,
    0xc, 0x50, 0xc, 0x50, 0xc, 0x50,

    /* U+0075 "u" */
    0xa7, 0x69, 0xa7, 0x69, 0xa7, 0x69, 0xa7, 0x69,
    0xa7, 0x69, 0x98, 0x88, 0x3d, 0xc2,

    /* U+0076 "v" */
    0xd4, 0x2d, 0xa6, 0x5a, 0x89, 0x78, 0x5b, 0x95,
    0x3e, 0xc3, 0xf, 0xe0, 0xe, 0xe0,

    /* U+0077 "w" */
    0xc4, 0xb7, 0x86, 0xa6, 0xc8, 0xa5, 0x97, 0xea,
    0xb3, 0x79, 0xeb, 0xd1, 0x5c, 0xbd, 0xe0, 0x3f,
    0x9e, 0xd0, 0x1f, 0x7d, 0xc0,

    /* U+0078 "x" */
    0x98, 0x1e, 0x4, 0xd7, 0x80, 0xe, 0xe3, 0x0,
    0xbf, 0x0, 0xe, 0xd4, 0x5, 0xa7, 0xa0, 0xa5,
    0x2f, 0x0,

    /* U+0079 "y" */
    0xc5, 0x4c, 0x89, 0x87, 0x3d, 0xc2, 0xe, 0xd0,
    0xa, 0x90, 0x8, 0x80, 0x8, 0x80,

    /* U+007A "z" */
    0xaf, 0xf7, 0x0, 0xe3, 0x5, 0xd0, 0xb, 0x60,
    0x2e, 0x0, 0x89, 0x0, 0xcf, 0xf7,

    /* U+007B "{" */
    0xd, 0x54, 0xc0, 0x6a, 0xe, 0x50, 0x6a, 0x4,
    0xc0, 0xd, 0x50,

    /* U+007C "|" */
    0x2e, 0xee, 0xee, 0xee, 0xe0,

    /* U+007D "}" */
    0xc8, 0x3, 0xd0, 0x1e, 0x0, 0xc7, 0x1e, 0x3,
    0xd0, 0xc8, 0x0,

    /* U+007E "~" */
    0x7b, 0x47, 0x32, 0xb5
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 25, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 30, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 7, .adv_w = 52, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 12, .adv_w = 66, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 63, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 44, .adv_w = 100, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 65, .adv_w = 66, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 29, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 86, .adv_w = 40, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 40, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 63, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 116, .adv_w = 63, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 124, .adv_w = 30, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 127, .adv_w = 43, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 129, .adv_w = 30, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 61, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 63, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 240, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 30, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 30, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 293, .adv_w = 63, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 303, .adv_w = 63, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 309, .adv_w = 63, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 319, .adv_w = 58, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 110, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 361, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 375, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 55, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 445, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 459, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 473, .adv_w = 30, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 41, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 66, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 509, .adv_w = 54, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 523, .adv_w = 87, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 558, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 61, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 586, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 60, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 57, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 642, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 656, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 670, .adv_w = 90, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 69, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 709, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 723, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 40, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 748, .adv_w = 61, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 762, .adv_w = 40, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 773, .adv_w = 63, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 779, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 782, .adv_w = 40, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 785, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 813, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 827, .adv_w = 65, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 841, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 855, .adv_w = 55, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 869, .adv_w = 62, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 883, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 897, .adv_w = 30, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 904, .adv_w = 41, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 915, .adv_w = 66, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 933, .adv_w = 54, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 947, .adv_w = 87, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 968, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 982, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 996, .adv_w = 60, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1010, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1024, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1038, .adv_w = 60, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1052, .adv_w = 57, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1066, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1080, .adv_w = 64, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1094, .adv_w = 90, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1115, .adv_w = 69, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1133, .adv_w = 63, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1147, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1161, .adv_w = 41, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1172, .adv_w = 80, .box_w = 1, .box_h = 9, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 1177, .adv_w = 41, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1188, .adv_w = 63, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 2, 3, 2,
    4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 6, 7, 8, 9, 0, 10,
    9, 0, 0, 11, 12, 13, 0, 0,
    9, 14, 15, 16, 17, 18, 11, 19,
    20, 21, 22, 23, 1, 0, 0, 0,
    0, 0, 24, 7, 8, 9, 0, 10,
    9, 0, 0, 11, 12, 13, 0, 0,
    9, 14, 15, 16, 17, 18, 11, 19,
    20, 21, 22, 23, 1, 0, 0, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 2, 3, 2,
    4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    6, 0, 7, 0, 8, 0, 0, 0,
    8, 0, 0, 9, 0, 0, 0, 0,
    8, 0, 10, 0, 11, 12, 13, 14,
    15, 16, 17, 18, 0, 0, 1, 0,
    0, 0, 7, 0, 8, 0, 0, 0,
    8, 0, 0, 9, 0, 0, 0, 0,
    8, 0, 10, 0, 11, 12, 13, 14,
    15, 16, 17, 18, 0, 0, 1, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, -2, -1, -10, -1, -10,
    -6, 0, -13, 0, 0, 0, 0, 0,
    0, 0, -2, 0, 0, 0, 0, -5,
    0, -2, -1, -2, -3, -2, 0, 0,
    0, -29, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -6, 0, 0, 0, 0,
    -3, 0, 0, 0, 0, 0, 0, -5,
    0, 0, 0, 0, 0, 0, 0, -4,
    0, 0, 0, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0, -2, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, -2, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -2,
    -2, 0, 1, -11, 0, 0, 0, 0,
    -2, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, -2, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -1, 0, -1,
    -1, 0, -1, 0, 0, 0, 0, 0,
    1, 0, -6, 0, 0, -5, 0, 0,
    1, 0, 0, -8, 0, -5, -3, 0,
    -10, 0, 0, -13, 0, 0, 0, 0,
    -2, 0, -3, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    -1, 0, 1, -10, -5, 0, -6, 0,
    -6, 0, -5, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -10, -2, 0,
    0, 0, -4, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -6,
    -1, 0, 0, 0, -2, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -2, 0, 0, 0, 0, -2,
    0, -2, -1, 0, 0, 0, 0, 0,
    0, 0, 1, -13, -3, 0, -3, 0,
    -8, -2, -6, -2, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, -5, 0, 0, 0, 0,
    0, -6, 0, -4, -2, 0, -8, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 24,
    .right_class_cnt     = 18,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_bebas_bold_10 = {
#else
lv_font_t ui_font_bebas_bold_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_BEBAS_BOLD_10*/

