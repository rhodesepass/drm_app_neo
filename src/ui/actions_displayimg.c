// 扩列图展示 专用
#include "ui.h"
#include "ui/actions_displayimg.h"
#include "utils/log.h"
#include "vars.h"
#include "ui/scr_transition.h"
#include <src/core/lv_group.h>
#include <dirent.h>
#include <src/lv_api_map_v8.h>
#include <string.h>
#include "lvgl.h"

static ui_displayimg_t g_displayimg;

// =========================================
// 自己添加的方法 START
// =========================================

static ui_displayimg_file_type_t displayimg_file_type(const char *filename){
    if (!filename) return UI_DISPLAYIMG_FILE_TYPE_INVALID;
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return UI_DISPLAYIMG_FILE_TYPE_INVALID;

    // Convert extension to lowercase
    char ext[8] = {0};
    int i = 0;
    dot++; // skip the dot
    while (dot[i] && i < 6) {
        ext[i] = (dot[i] >= 'A' && dot[i] <= 'Z') ? (dot[i] + 32) : dot[i];
        i++;
    }
    ext[i] = '\0';

    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return UI_DISPLAYIMG_FILE_TYPE_JPG;
    if (strcmp(ext, "png") == 0) return UI_DISPLAYIMG_FILE_TYPE_PNG;
    if (strcmp(ext, "bmp") == 0) return UI_DISPLAYIMG_FILE_TYPE_BMP;
    if (strcmp(ext, "gif") == 0) return UI_DISPLAYIMG_FILE_TYPE_GIF;

    return UI_DISPLAYIMG_FILE_TYPE_INVALID;
}

static void display_img(ui_displayimg_file_t *file){
    lv_obj_clean(objects.dispimg_container);
    if(file->is_gif){
        lv_obj_t *gif_obj = lv_gif_create(objects.dispimg_container);
        lv_gif_set_color_format(gif_obj, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(gif_obj, file->img_path);
        lv_obj_center(gif_obj);
        lv_obj_move_background(gif_obj);

    }else{
        lv_obj_t *img_obj = lv_image_create(objects.dispimg_container);
        lv_image_set_src(img_obj, file->img_path);
        lv_obj_center(img_obj);
        lv_obj_move_background(img_obj);
    }
    lv_obj_move_background(objects.dispimg_container);

}

void ui_displayimg_init(){
    g_displayimg.count = 0;
    g_displayimg.index = 0;

    DIR *dir = opendir(DISPLAYIMG_PATH);
    if(dir == NULL){
        log_error("ui_displayimg_init: open dir %s failed", DISPLAYIMG_PATH);
        return;
    }
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        if(entry->d_type == DT_REG){
            if(displayimg_file_type(entry->d_name) == UI_DISPLAYIMG_FILE_TYPE_INVALID){
                continue;
            }
            snprintf(g_displayimg.files[g_displayimg.count].img_path, DISPLAYIMG_MAX_PATH_LENGTH, "A:%s%s", DISPLAYIMG_PATH, entry->d_name);
            g_displayimg.files[g_displayimg.count].is_gif = (displayimg_file_type(entry->d_name) == UI_DISPLAYIMG_FILE_TYPE_GIF);
            g_displayimg.count++;
        }
    }
    closedir(dir);
    log_info("ui_displayimg_init: found %d valid displayimg files", g_displayimg.count);
    if(g_displayimg.count != 0){
        display_img(&g_displayimg.files[0]);
    }
    return;
}

void ui_displayimg_key_event(uint32_t key){
    if(g_displayimg.count == 0){
        return;
    }
    switch(key){
        case LV_KEY_LEFT:
            g_displayimg.index--;
            if(g_displayimg.index < 0){
                g_displayimg.index = 0;
            }
            break;
        case LV_KEY_RIGHT:
            g_displayimg.index++;
            if(g_displayimg.index >= g_displayimg.count){
                g_displayimg.index = g_displayimg.count - 1;
            }
            break;
        default:
            break;
    }
    display_img(&g_displayimg.files[g_displayimg.index]);
}

void ui_displayimg_force_dispimg(const char *path){
    ui_displayimg_file_type_t type = displayimg_file_type(path);
    if(type == UI_DISPLAYIMG_FILE_TYPE_INVALID){
        return;
    }
    static ui_displayimg_file_t file;
    snprintf(file.img_path, DISPLAYIMG_MAX_PATH_LENGTH, "A:%s", path);
    file.is_gif = (type == UI_DISPLAYIMG_FILE_TYPE_GIF);
    display_img(&file);
}

// =========================================
// EEZ 回调 START
// =========================================

const char *get_var_displayimg_size_lbl(){
    static char buf[128];
    snprintf(buf, sizeof(buf), "%d/%d", g_displayimg.index + 1, g_displayimg.count);
    return buf;
}
void set_var_displayimg_size_lbl(const char *value){
    return;
}

void action_displayimg_key(lv_event_t * e){
    log_debug("action_displayimg_key");
}

void action_show_dispimg(lv_event_t * e){
    log_debug("action_show_dispimg");
    lv_obj_t* obj = lv_event_get_target(e);
    lv_obj_remove_state(obj, LV_STATE_PRESSED);
    ui_schedule_screen_transition(curr_screen_t_SCREEN_DISPLAYIMG);
}

bool get_var_dispimg_show_warning(){
    return g_displayimg.count != 0;
}

void set_var_dispimg_show_warning(bool value){
    return;
}
const char *get_var_dispimg_path(){
    if(g_displayimg.count == 0){
        return "ERROR";
    }
    return g_displayimg.files[g_displayimg.index].img_path;
}

void set_var_dispimg_path(const char *value){
    return;
}