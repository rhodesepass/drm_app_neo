#include "fbdraw.h"
#include "log.h"
#include "config.h"
#include <fcntl.h>
#include <unistd.h>

// inline uint32_t GETINDEX(fbdraw_t *fbdraw,int x, int y){
//     #ifdef UI_ROTATION_180
//         return (fbdraw->fb_height - 1 - y) * fbdraw->fb_width + (fbdraw->fb_width - 1 - x);    
//     #else
//         return y * fbdraw->fb_width + x;
//     #endif
// }

#ifdef UI_ROTATION_180
    #define GETINDEX(draw,x,y) (((draw)->fb_height - 1 - (y)) * (draw)->fb_width + ((draw)->fb_width - 1 - (x)))
#else
    #define GETINDEX(draw,x,y) ((y) * (draw)->fb_width + (x))
#endif

void fbdraw_fill_rect(fbdraw_t *fbdraw, int x, int y, int w, int h, int color)
{
    uint32_t *vaddr = fbdraw->vaddr;

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            // log_info("x: %d, y: %d, index: %d", x + j, y + i, GETINDEX(fbdraw, x + j, y + i));
            vaddr[GETINDEX(fbdraw, x + j, y + i)] = color;
        }
    }
}

void fbdraw_draw_bitmap_1_bit(fbdraw_t *fbdraw, int x, int y, unsigned char *bitmap, int w, int h, int fgcolor, int bgcolor)
{
    uint32_t *vaddr = fbdraw->vaddr;
    int byte_width = (w + 7) / 8;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int byte_idx = row * byte_width + (col / 8);
            int bit_idx = 7 - (col % 8);
            int bit = (bitmap[byte_idx] >> bit_idx) & 0x01;

            int px = x + col;
            int py = y + row;

            if (px < 0 || py < 0 || px >= fbdraw->fb_width || py >= fbdraw->fb_height)
                continue;

            vaddr[GETINDEX(fbdraw, px, py)] = bit ? fgcolor : bgcolor;
        }
    }
}



int fbdraw_argb_bitmap_from_file(fbdraw_t *fbdraw, int x,int y,int width,int height, const char *filename){

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        log_error("failed to open file: %s", filename);
        return -1;
    }
    int fsize = lseek(fd, 0, SEEK_END);
    if (fsize != width * height * 4) {
        log_error("file size mismatch: %d != %d * %d * 4", fsize, width, height);
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    
    read(fd, fbdraw->vaddr, width * height * 4);
    close(fd);
    return 0;
}

int fbdraw_argb_bitmap_region_from_file(fbdraw_t *fbdraw, int x,int y,int width,int height, int reg_x,int reg_y,int reg_width,int reg_height, const char *filename){
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        log_error("failed to open file: %s", filename);
        return -1;
    }
    int fsize = lseek(fd, 0, SEEK_END);
    if (fsize != width * height * 4) {
        log_error("file size mismatch: %d != %d * %d * 4", fsize, width, height);
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);

    for (int row = 0; row < reg_height; row++) {
        int file_row = reg_y + row;
        if (file_row < 0 || file_row >= height)
            continue; // skip out-of-bounds file row

        if (reg_x < 0 || reg_x + reg_width > width)
            continue; // skip out-of-bounds region

        off_t offset = (file_row * width + reg_x) * 4;
        if (lseek(fd, offset, SEEK_SET) < 0) {
            log_error("lseek failed");
            close(fd);
            return -1;
        }

        uint32_t rowbuf[reg_width];
        ssize_t n = read(fd, rowbuf, reg_width * 4);
        if (n != reg_width * 4) {
            log_error("short read or error: %zd/%d", n, reg_width * 4);
            close(fd);
            return -1;
        }

        int py = y + row;
        if (py < 0 || py >= fbdraw->fb_height)
            continue;
        for (int col = 0; col < reg_width; col++) {
            int px = x + col;
            if (px < 0 || px >= fbdraw->fb_width)
                continue;
            fbdraw->vaddr[(fbdraw->fb_height-py) * fbdraw->fb_width + px] = rowbuf[col];
        }
    }
    close(fd);
    return 0;
}

int fbdraw_argb_bitmap_from_file_with_delay(fbdraw_t *fbdraw, int x,int y,int width,int height, const char *filename,int row_delay){

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        log_error("failed to open file: %s", filename);
        return -1;
    }
    int fsize = lseek(fd, 0, SEEK_END);
    if (fsize != width * height * 4) {
        log_error("file size mismatch: %d != %d * %d * 4", fsize, width, height);
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    for (int row = 0; row < height; row++) {
        read(fd, ((uint8_t*)fbdraw->vaddr) + row * width * 4, width * 4);
        usleep(row_delay);
    }
    close(fd);
    return 0;
}
