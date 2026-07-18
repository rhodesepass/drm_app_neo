#!/bin/sh
# 重生成 C8 烘焙调色板(src/render/c8pal_baked.h)与 assets/ 下 6 张
# cacheasset 素材的 .c8 索引图(两档分辨率,先缩后抖)。
set -e
cd "$(dirname "$0")"

gcc -O2 -o png2c8 png2c8.c -lm

./png2c8 ../src/render/c8pal_baked.h ../assets \
    ../assets/ak_bar.png \
    ../assets/btm_left_bar.png \
    ../assets/top_left_rect.png \
    ../assets/top_left_rhodes.png \
    ../assets/top_right_bar.png \
    ../assets/top_right_arrow.png
