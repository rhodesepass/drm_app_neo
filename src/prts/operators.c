#include "prts/prts.h"
#include "prts/operators.h"
#include "utils/cJSON.h"
#include "utils/stb_image.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include "utils/log.h"
#include "utils/misc.h"
#include "utils/respath.h"
#include "ui/font_registry.h"

// 可选图片通用校验规则（优化版：仅检查文件存在性，不加载图片）：
// - json字段不存在 / 非字符串 / 空字符串 => 视为不存在，dst置空
// - 若存在且不为空：join到绝对路径，检查文件存在且可读
// - 不满足 => 视为不存在，dst置空
// 注意：图片格式/尺寸验证推迟到实际加载时进行
static void validate_optional_image_path(
    prts_t *prts,
    const char *op_dir,
    const char *field_for_log,
    const char *rel_path,
    char *dst,
    size_t dst_sz
) {
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';

    if (!rel_path || rel_path[0] == '\0') {
        return;
    }

    char abs_path[256];
    abs_path[0] = '\0';
    join_path(abs_path, sizeof(abs_path), op_dir, rel_path);

    // 仅检查文件存在和可读性，不加载图片（性能优化）
    if (!file_exists_readable(abs_path)) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, (char*)field_for_log, PARSE_LOG_WARN);
        return;
    }

    safe_strcpy(dst, dst_sz, abs_path);
}

static int parse_transition_obj(
    prts_t *prts,
    const char *op_dir,
    const char *which,
    cJSON *tr_obj,
    oltr_params_t *out
) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    out->type = TRANSITION_TYPE_NONE;
    out->background_color = 0xFF000000u;
    out->image_path[0] = '\0';

    if (!tr_obj) {
        // 可选：不存在则认为 none
        return 0;
    }
    if (!cJSON_IsObject(tr_obj)) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, (char*)which, PARSE_LOG_ERROR);
        return -1;
    }

    const char *t = json_get_string(tr_obj, "type");
    if (!t) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "transition 缺少 type", PARSE_LOG_ERROR);
        return -1;
    }

    if (strcmp(t, "none") == 0) {
        out->type = TRANSITION_TYPE_NONE;
        return 0;
    }
    if (strcmp(t, "fade") == 0) out->type = TRANSITION_TYPE_FADE;
    else if (strcmp(t, "move") == 0) out->type = TRANSITION_TYPE_MOVE;
    else if (strcmp(t, "swipe") == 0) out->type = TRANSITION_TYPE_SWIPE;
    else {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "transition.type 不合法", PARSE_LOG_ERROR);
        return -1;
    }

    cJSON *opt = cJSON_GetObjectItem(tr_obj, "options");
    if (!opt || !cJSON_IsObject(opt)) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "transition.type!=none 但 options 不存在", PARSE_LOG_ERROR);
        return -1;
    }

    out->duration = json_get_int(opt, "duration", 0);
    if (out->duration <= 0) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "transition.options.duration<=0", PARSE_LOG_ERROR);
        return -1;
    }

    const char *bg = json_get_string(opt, "background_color");
    out->background_color = (bg && is_hex_color_6(bg)) ? parse_rgbff(bg) : 0xFF000000u;

    const char *img = json_get_string(opt, "image");
    // 可选图片通用校验规则
    char warn_msg[128];
    snprintf(warn_msg, sizeof(warn_msg), "%s.options.image 校验失败，按不存在处理", which);
    validate_optional_image_path(prts, op_dir, warn_msg, img, out->image_path, sizeof(out->image_path));
    return 0;
}


// ===== overlay.type=custom 的元素解析 =====

static int parse_element_type(const char *s, opinfo_element_type_t *out) {
    if (strcmp(s, "text") == 0)             *out = OPINFO_EL_TEXT;
    else if (strcmp(s, "text_rot90") == 0)  *out = OPINFO_EL_TEXT_ROT90;
    else if (strcmp(s, "image") == 0)       *out = OPINFO_EL_IMAGE;
    else if (strcmp(s, "rect") == 0)        *out = OPINFO_EL_RECT;
    else if (strcmp(s, "barcode") == 0)     *out = OPINFO_EL_BARCODE;
    else if (strcmp(s, "corner_fade") == 0) *out = OPINFO_EL_CORNER_FADE;
    else return -1;
    return 0;
}

// speed 的默认值随动画类型（语义见 opinfo.h）
static const struct {
    const char *name;
    opinfo_anim_t anim;
    int default_speed;
} k_anim_map[] = {
    { "none",       OPINFO_ANIM_NONE,       0  },
    { "typewriter", OPINFO_ANIM_TYPEWRITER, 3  },
    { "eink",       OPINFO_ANIM_EINK,       15 },
    { "fade",       OPINFO_ANIM_FADE,       8  },
    { "wipe",       OPINFO_ANIM_WIPE,       40 },
    { "scroll",     OPINFO_ANIM_SCROLL,     1  },
    { "grow",       OPINFO_ANIM_GROW,       10 },
    { "move",       OPINFO_ANIM_MOVE,       15 },
    { "scramble",   OPINFO_ANIM_SCRAMBLE,   2  },
    { "blink",      OPINFO_ANIM_BLINK,      15 },
    { "sprite",     OPINFO_ANIM_SPRITE,     4  },
    { "sway",       OPINFO_ANIM_SWAY,       60 },
};

static bool anim_valid_for_type(opinfo_anim_t anim, opinfo_element_type_t type) {
    switch (anim) {
    case OPINFO_ANIM_NONE:       return true;
    case OPINFO_ANIM_TYPEWRITER: return type == OPINFO_EL_TEXT;
    case OPINFO_ANIM_EINK:       return type == OPINFO_EL_IMAGE || type == OPINFO_EL_RECT || type == OPINFO_EL_BARCODE;
    case OPINFO_ANIM_FADE:       return type == OPINFO_EL_IMAGE || type == OPINFO_EL_TEXT || type == OPINFO_EL_RECT;
    case OPINFO_ANIM_WIPE:       return type == OPINFO_EL_RECT || type == OPINFO_EL_IMAGE;
    case OPINFO_ANIM_SCROLL:     return type == OPINFO_EL_IMAGE;
    case OPINFO_ANIM_GROW:       return type == OPINFO_EL_CORNER_FADE;
    case OPINFO_ANIM_SCRAMBLE:   return type == OPINFO_EL_TEXT;
    case OPINFO_ANIM_SPRITE:     return type == OPINFO_EL_IMAGE;
    case OPINFO_ANIM_MOVE:
    case OPINFO_ANIM_BLINK:
    case OPINFO_ANIM_SWAY:
        return type != OPINFO_EL_CORNER_FADE;
    }
    return false;
}

static int parse_custom_element(prts_t *prts, const char *op_dir, cJSON *jel, olopinfo_element_t *el) {
    overlay_opinfo_element_init(el);

    if (!jel || !cJSON_IsObject(jel)) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "overlay.options.elements 含非对象项", PARSE_LOG_ERROR);
        return -1;
    }

    const char *t = json_get_string(jel, "type");
    if (!t || parse_element_type(t, &el->type) != 0) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "element.type 缺失或不合法", PARSE_LOG_ERROR);
        return -1;
    }

    el->x = json_get_int(jel, "x", 0);
    el->y = json_get_int(jel, "y", 0);
    el->w = json_get_int(jel, "w", 0);
    el->h = json_get_int(jel, "h", 0);
    el->start_frame = json_get_int(jel, "start_frame", 0);
    if (el->start_frame < 0) el->start_frame = 0;
    el->end_frame = json_get_int(jel, "end_frame", 0);
    if (el->end_frame < 0) el->end_frame = 0;
    if (el->end_frame > 0 && el->end_frame <= el->start_frame) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "element.end_frame<=start_frame，按永不退场处理", PARSE_LOG_WARN);
        el->end_frame = 0;
    }
    el->border_width = json_get_int(jel, "border_width", 0);
    if (el->border_width < 0) el->border_width = 0;
    el->from_dx = json_get_int(jel, "from_dx", 0);
    el->from_dy = json_get_int(jel, "from_dy", 0);
    el->frames = json_get_int(jel, "frames", 0);
    if (el->frames < 0) el->frames = 0;

    const char *dir = json_get_string(jel, "direction");
    if (dir) {
        if (strcmp(dir, "ltr") == 0)      el->wipe_dir = OPINFO_WIPE_LTR;
        else if (strcmp(dir, "rtl") == 0) el->wipe_dir = OPINFO_WIPE_RTL;
        else if (strcmp(dir, "ttb") == 0) el->wipe_dir = OPINFO_WIPE_TTB;
        else if (strcmp(dir, "btt") == 0) el->wipe_dir = OPINFO_WIPE_BTT;
        else parse_log_file(prts->parse_log_f, (char*)op_dir, "element.direction 不合法，按 ltr 处理", PARSE_LOG_WARN);
    }

    const char *anchor = json_get_string(jel, "anchor");
    if (anchor) {
        if (strcmp(anchor, "tl") == 0)      el->anchor = OPINFO_ANCHOR_TL;
        else if (strcmp(anchor, "tr") == 0) el->anchor = OPINFO_ANCHOR_TR;
        else if (strcmp(anchor, "bl") == 0) el->anchor = OPINFO_ANCHOR_BL;
        else if (strcmp(anchor, "br") == 0) el->anchor = OPINFO_ANCHOR_BR;
        else parse_log_file(prts->parse_log_f, (char*)op_dir, "element.anchor 不合法，按 tl 处理", PARSE_LOG_WARN);
    }

    const char *txt = json_get_string(jel, "text");
    if (txt) safe_strcpy(el->text, sizeof(el->text), txt);

    const char *font = json_get_string(jel, "font");
    if (font) {
        if (strcmp(font, "body") == 0)         el->font_role = FONT_BODY;
        else if (strcmp(font, "title") == 0)   el->font_role = FONT_TITLE;
        else if (strcmp(font, "display") == 0) el->font_role = FONT_DISPLAY;
        else if (strcmp(font, "icon") == 0)    el->font_role = FONT_ICON;
        else parse_log_file(prts->parse_log_f, (char*)op_dir, "element.font 不合法，按 body 处理", PARSE_LOG_WARN);
    }
    el->font_size = json_get_int(jel, "font_size", el->font_size);
    el->line_height = json_get_int(jel, "line_height", 0);
    el->letter_space = json_get_int(jel, "letter_space", 0);
    el->faux_bold = json_get_bool(jel, "faux_bold", false);
    el->bold_split = json_get_bool(jel, "bold_split", false);

    const char *color = json_get_string(jel, "color");
    if (color && is_hex_color_6(color)) el->color = parse_rgbff(color);

    const char *img = json_get_string(jel, "image");
    validate_optional_image_path(prts, op_dir, "element.image 校验失败，按不存在处理",
                                 img, el->image_path, sizeof(el->image_path));

    int default_speed = 0;
    const char *anim = json_get_string(jel, "animation");
    if (anim) {
        bool found = false;
        for (size_t i = 0; i < sizeof(k_anim_map) / sizeof(k_anim_map[0]); i++) {
            if (strcmp(anim, k_anim_map[i].name) == 0) {
                el->anim = k_anim_map[i].anim;
                default_speed = k_anim_map[i].default_speed;
                found = true;
                break;
            }
        }
        if (!found) {
            parse_log_file(prts->parse_log_f, (char*)op_dir, "element.animation 不合法，按 none 处理", PARSE_LOG_WARN);
            el->anim = OPINFO_ANIM_NONE;
        }
    }
    if (!anim_valid_for_type(el->anim, el->type)) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "element.animation 与 type 不匹配，按 none 处理", PARSE_LOG_WARN);
        el->anim = OPINFO_ANIM_NONE;
        default_speed = 0;
    }
    if (el->anim == OPINFO_ANIM_MOVE && el->from_dx == 0 && el->from_dy == 0) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "move 元素缺少 from_dx/from_dy，按 none 处理", PARSE_LOG_WARN);
        el->anim = OPINFO_ANIM_NONE;
        default_speed = 0;
    }
    if (el->anim == OPINFO_ANIM_SWAY && el->from_dx == 0 && el->from_dy == 0) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "sway 元素缺少 from_dx/from_dy（摆幅），按 none 处理", PARSE_LOG_WARN);
        el->anim = OPINFO_ANIM_NONE;
        default_speed = 0;
    }
    if (el->anim == OPINFO_ANIM_SPRITE && el->frames < 2) {
        parse_log_file(prts->parse_log_f, (char*)op_dir, "sprite 元素 frames<2，按 none 处理", PARSE_LOG_WARN);
        el->anim = OPINFO_ANIM_NONE;
        default_speed = 0;
    }
    el->speed = json_get_int(jel, "speed", 0);
    if (el->speed <= 0) el->speed = default_speed;

    // 类型特定的必填项与默认值
    switch (el->type) {
    case OPINFO_EL_RECT:
    case OPINFO_EL_TEXT_ROT90:
        if (el->w <= 0 || el->h <= 0) {
            parse_log_file(prts->parse_log_f, (char*)op_dir, "rect/text_rot90 元素缺少 w/h", PARSE_LOG_ERROR);
            return -1;
        }
        break;
    case OPINFO_EL_BARCODE:
        if (el->w <= 0) el->w = 50;
        if (el->h <= 0) el->h = 180;
        if (el->text[0] == '\0') {
            parse_log_file(prts->parse_log_f, (char*)op_dir, "barcode 元素缺少 text", PARSE_LOG_ERROR);
            return -1;
        }
        break;
    case OPINFO_EL_CORNER_FADE:
        if (el->w <= 1) el->w = 192; // 目标半径
        break;
    case OPINFO_EL_IMAGE:
        if (el->image_path[0] == '\0') {
            parse_log_file(prts->parse_log_f, (char*)op_dir, "image 元素图片不可用，将不绘制", PARSE_LOG_WARN);
        }
        break;
    default:
        break;
    }

    return 0;
}

void prts_operator_entry_free(prts_operator_entry_t* operator) {
    if (!operator) return;
    overlay_opinfo_free_elements(&operator->opinfo_params);
}

int prts_operator_try_load(prts_t *prts,prts_operator_entry_t* operator,char * path,prts_source_t source,int index){
    if (!path || strlen(path) == 0) {
        return -1;
    }
    if (!operator) {
        parse_log_file(prts->parse_log_f, path, "operator 指针为空", PARSE_LOG_ERROR);
        return -1;
    }
    memset(operator, 0, sizeof(*operator));
    operator->index = index;
    operator->source = source;
    operator->opinfo_params.type = OPINFO_TYPE_NONE;
    operator->transition_in.type = TRANSITION_TYPE_NONE;
    operator->transition_in.background_color = 0xFF000000u;
    operator->transition_loop.type = TRANSITION_TYPE_NONE;
    operator->transition_loop.background_color = 0xFF000000u;

    char cfg_path[256];
    join_path(cfg_path, sizeof(cfg_path), path, PRTS_ASSET_CONFIG_FILENAME);

    size_t json_len = 0;
    char *buf = read_file_all(cfg_path, &json_len);
    if (!buf) {
        parse_log_file(prts->parse_log_f, path, PRTS_ASSET_CONFIG_FILENAME "不存在或读取失败", PARSE_LOG_ERROR);
        return -1;
    }

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if(json == NULL){
        parse_log_file(prts->parse_log_f, path, PRTS_ASSET_CONFIG_FILENAME "解析失败", PARSE_LOG_ERROR);
        return -1;
    }

    // ===== version =====
    cJSON *ver = cJSON_GetObjectItem(json, "version");
    if (!ver || !cJSON_IsNumber(ver) || ver->valueint != PRTS_ASSET_VERSION_NUMBER) {
        parse_log_file(prts->parse_log_f, path, "version 校验失败", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    // ===== top-level basic fields =====
    const char *name = json_get_string(json, "name");
    if (!name || name[0] == '\0') {
        // name 可选：没有就用文件夹名称
        name = path_basename(path);
    }
    safe_strcpy(operator->operator_name, sizeof(operator->operator_name), name);

    const char *uuid_str = json_get_string(json, "uuid");
    if (!uuid_str || uuid_str[0] == '\0') {
        parse_log_file(prts->parse_log_f, path, "缺少字段 uuid", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    if (uuid_parse(uuid_str, &operator->uuid) != 0) {
        parse_log_file(prts->parse_log_f, path, "uuid 解析失败", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    const char *desc = json_get_string(json, "description");
    if (!desc || desc[0] == '\0') desc = "(无描述)";
    safe_strcpy(operator->description, sizeof(operator->description), desc);

    // icon: 可选，输出为 LVGL path（A: 前缀）；不存在则用默认 icon
    const char *icon = json_get_string(json, "icon");
    if (!icon || icon[0] == '\0') {
        safe_strcpy(operator->icon_path, sizeof(operator->icon_path), respath_lvfs(RES_DEFAULT_ICON_FILE));
    } else {
        char abs_icon[256];
        abs_icon[0] = '\0';
        join_path(abs_icon, sizeof(abs_icon), path, icon);
        if (!file_exists_readable(abs_icon)) {
            parse_log_file(prts->parse_log_f, path, "icon 文件不存在，使用默认icon", PARSE_LOG_WARN);
            safe_strcpy(operator->icon_path, sizeof(operator->icon_path), respath_lvfs(RES_DEFAULT_ICON_FILE));
        } else {
            set_lvgl_path(operator->icon_path, sizeof(operator->icon_path), abs_icon);
        }
    }

    // screen 字段只用来判定素材原生分辨率(决定图片是否软件放大)，不再据此拒绝素材。
    // 缺失/无法识别时退回本机固件分辨率，按原样处理。
#if defined(USE_360_640_SCREEN)
    display_type_t native_disp = DISPLAY_360_640;
#elif defined(USE_480_854_SCREEN)
    display_type_t native_disp = DISPLAY_480_854;
#elif defined(USE_720_1280_SCREEN)
    display_type_t native_disp = DISPLAY_720_1280;
#endif
    const char *screen = json_get_string(json, "screen");
    if (screen && strcmp(screen, "360x640") == 0) {
        operator->disp_type = DISPLAY_360_640;
    } else if (screen && strcmp(screen, "480x854") == 0) {
        operator->disp_type = DISPLAY_480_854;
    } else if (screen && strcmp(screen, "720x1280") == 0) {
        operator->disp_type = DISPLAY_720_1280;
    } else {
        operator->disp_type = native_disp;
    }


    // ===== loop / intro videos =====
    cJSON *loop = cJSON_GetObjectItem(json, "loop");
    if (!loop || !cJSON_IsObject(loop)) {
        parse_log_file(prts->parse_log_f, path, "缺少对象 loop", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    const char *loop_file = json_get_string(loop, "file");
    if (!loop_file || loop_file[0] == '\0') {
        parse_log_file(prts->parse_log_f, path, "缺少 loop.file", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }
    join_path(operator->loop_video.path, sizeof(operator->loop_video.path), path, loop_file);
    if (!file_exists_readable(operator->loop_video.path)) {
        parse_log_file(prts->parse_log_f, path, "loop.file 文件不存在", PARSE_LOG_ERROR);
        cJSON_Delete(json);
        return -1;
    }

    cJSON *intro = cJSON_GetObjectItem(json, "intro");
    if (!intro || !cJSON_IsObject(intro)) {
        operator->intro_video.enabled = false;
        operator->intro_video.duration = 0;
        operator->intro_video.path[0] = '\0';
    } else {
        operator->intro_video.enabled = json_get_bool(intro, "enabled", false);
        operator->intro_video.duration = json_get_int(intro, "duration", 0);
        const char *intro_file = json_get_string(intro, "file");
        if (operator->intro_video.enabled) {
            if (!intro_file || intro_file[0] == '\0') {
                parse_log_file(prts->parse_log_f, path, "intro.enabled=true 但缺少 intro.file", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            if (operator->intro_video.duration <= 0) {
                parse_log_file(prts->parse_log_f, path, "intro.enabled=true 但 duration<=0", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            join_path(operator->intro_video.path, sizeof(operator->intro_video.path), path, intro_file);
            if (!file_exists_readable(operator->intro_video.path)) {
                parse_log_file(prts->parse_log_f, path, "intro.file 文件不存在", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
        } else {
            operator->intro_video.duration = 0;
            operator->intro_video.path[0] = '\0';
        }
    }

    // ===== transitions (only keep params: duration/image/background_color) =====
    cJSON *tr_in = cJSON_GetObjectItem(json, "transition_in");
    if (parse_transition_obj(prts, path, "transition_in", tr_in, &operator->transition_in) != 0) {
        cJSON_Delete(json);
        return -1;
    }
    cJSON *tr_lp = cJSON_GetObjectItem(json, "transition_loop");
    if (parse_transition_obj(prts, path, "transition_loop", tr_lp, &operator->transition_loop) != 0) {
        cJSON_Delete(json);
        return -1;
    }

    // ===== overlay / opinfo (图片不加载，只填路径) =====
    cJSON *overlay = cJSON_GetObjectItem(json, "overlay");
    if (overlay && cJSON_IsObject(overlay)) {
        const char *ov_type = json_get_string(overlay, "type");
        cJSON *opt = cJSON_GetObjectItem(overlay, "options");

        if (!ov_type) {
            // 缺 type 时按 none 处理
            operator->opinfo_params.type = OPINFO_TYPE_NONE;
        } else if (strcmp(ov_type, "none") == 0) {
            operator->opinfo_params.type = OPINFO_TYPE_NONE;
        } else if (strcmp(ov_type, "arknights") == 0) {
            operator->opinfo_params.type = OPINFO_TYPE_ARKNIGHTS;
            if (!opt || !cJSON_IsObject(opt)) {
                parse_log_file(prts->parse_log_f, path, "overlay.type=arknights 但 options 不存在", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            // appear_time
            int appear_time = json_get_int(opt, "appear_time", 0);
            if (appear_time <= 0) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.appear_time<=0", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            operator->opinfo_params.appear_time = appear_time;

            const char *opn = json_get_string(opt, "operator_name");
            const char *opc = json_get_string(opt, "operator_code");
            const char *bct = json_get_string(opt, "barcode_text");
            const char *aux = json_get_string(opt, "aux_text");
            const char *stf = json_get_string(opt, "staff_text");

            safe_strcpy(operator->opinfo_params.operator_name, sizeof(operator->opinfo_params.operator_name), (opn && opn[0]) ? opn : "OPERATOR");
            safe_strcpy(operator->opinfo_params.operator_code, sizeof(operator->opinfo_params.operator_code), (opc && opc[0]) ? opc : "ARKNIGHT - UNK0");
            safe_strcpy(operator->opinfo_params.barcode_text, sizeof(operator->opinfo_params.barcode_text), (bct && bct[0]) ? bct : "OPERATOR - ARKNIGHTS");
            safe_strcpy(operator->opinfo_params.aux_text, sizeof(operator->opinfo_params.aux_text),
                        (aux && aux[0]) ? aux : "Operator of Rhodes Island\nUndefined/Rhodes Island\n Hypergryph");
            safe_strcpy(operator->opinfo_params.staff_text, sizeof(operator->opinfo_params.staff_text), (stf && stf[0]) ? stf : "STAFF");

            const char *color = json_get_string(opt, "color");
            operator->opinfo_params.color = (color && is_hex_color_6(color)) ? parse_rgbff(color) : 0xFF000000u;

            const char *logo = json_get_string(opt, "logo");
            validate_optional_image_path(prts, path, "overlay.options.logo 校验失败，按不存在处理", logo, operator->opinfo_params.logo_path, sizeof(operator->opinfo_params.logo_path));
            const char *cls = json_get_string(opt, "operator_class_icon");
            validate_optional_image_path(prts, path, "overlay.options.operator_class_icon 校验失败，按不存在处理", cls, operator->opinfo_params.class_path, sizeof(operator->opinfo_params.class_path));

            const char *rhodes = json_get_string(opt, "top_left_rhodes");
            safe_strcpy(operator->opinfo_params.rhodes_text, sizeof(operator->opinfo_params.rhodes_text),
                        (rhodes && rhodes[0]) ? rhodes : "");

            const char *trbt = json_get_string(opt, "top_right_bar_text");
            safe_strcpy(operator->opinfo_params.top_right_bar_text, sizeof(operator->opinfo_params.top_right_bar_text),
                        (trbt && trbt[0]) ? trbt : "");

        } else if (strcmp(ov_type, "custom") == 0) {
            operator->opinfo_params.type = OPINFO_TYPE_CUSTOM;
            if (!opt || !cJSON_IsObject(opt)) {
                parse_log_file(prts->parse_log_f, path, "overlay.type=custom 但 options 不存在", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            int appear_time = json_get_int(opt, "appear_time", 0);
            if (appear_time <= 0) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.appear_time<=0", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            operator->opinfo_params.appear_time = appear_time;
            // 可选：进场滑入时长，缺省(<=0)由引擎用默认 1s
            operator->opinfo_params.duration = json_get_int(opt, "duration", 0);

            cJSON *arr = cJSON_GetObjectItem(opt, "elements");
            int count = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
            if (count <= 0) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.elements 缺失或为空", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            if (count > OPINFO_ELEMENTS_MAX) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.elements 超过上限，超出部分忽略", PARSE_LOG_WARN);
                count = OPINFO_ELEMENTS_MAX;
            }

            olopinfo_element_t *els = calloc((size_t)count, sizeof(olopinfo_element_t));
            if (!els) {
                parse_log_file(prts->parse_log_f, path, "elements 内存分配失败", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            for (int i = 0; i < count; i++) {
                if (parse_custom_element(prts, path, cJSON_GetArrayItem(arr, i), &els[i]) != 0) {
                    free(els);
                    cJSON_Delete(json);
                    return -1;
                }
            }
            operator->opinfo_params.elements = els;
            operator->opinfo_params.element_count = count;

        } else if (strcmp(ov_type, "image") == 0) {
            operator->opinfo_params.type = OPINFO_TYPE_IMAGE;
            if (!opt || !cJSON_IsObject(opt)) {
                parse_log_file(prts->parse_log_f, path, "overlay.type=image 但 options 不存在", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            int appear_time = json_get_int(opt, "appear_time", 0);
            if (appear_time <= 0) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.appear_time<=0", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
            operator->opinfo_params.appear_time = appear_time;
            operator->opinfo_params.duration = json_get_int(opt, "duration", 0);
            if (operator->opinfo_params.duration <= 0) {
                parse_log_file(prts->parse_log_f, path, "overlay.options.duration<=0", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }

            const char *img = json_get_string(opt, "image");
            validate_optional_image_path(prts, path, "overlay.options.image 校验失败，按不存在处理", img, operator->opinfo_params.image_path, sizeof(operator->opinfo_params.image_path));

            // 翻译成单个静态 image 元素
            if (overlay_opinfo_build_image_elements(&operator->opinfo_params) != 0) {
                parse_log_file(prts->parse_log_f, path, "image 元素列表构建失败", PARSE_LOG_ERROR);
                cJSON_Delete(json);
                return -1;
            }
        } else {
            parse_log_file(prts->parse_log_f, path, "overlay.type 不合法", PARSE_LOG_ERROR);
            cJSON_Delete(json);
            return -1;
        }
    }

    // 用户图片按 素材原生倍率 vs 固件倍率 的比值软件缩放:
    //   360 基准素材在 720p 档 → 放大 UI_SCALE；720 基准素材在 360 档 → 缩小 2。
    int asset_scale = (operator->disp_type == DISPLAY_720_1280) ? 2 : 1;
    int fw_scale = UI_SCALE;
    int upscale   = (fw_scale > asset_scale) ? fw_scale / asset_scale : 1;
    int downscale = (asset_scale > fw_scale) ? asset_scale / fw_scale : 1;
    operator->opinfo_params.src_upscale = upscale;
    operator->opinfo_params.src_downscale = downscale;
    operator->transition_in.src_upscale = upscale;
    operator->transition_in.src_downscale = downscale;
    operator->transition_loop.src_upscale = upscale;
    operator->transition_loop.src_downscale = downscale;

    cJSON_Delete(json);
    return 0;
}

int prts_operator_scan_assets(prts_t *prts,char* dirpath,prts_source_t source){
    int error_cnt = 0;
    char path[128];
    DIR *dir = opendir(dirpath);
    if (!dir) {
        parse_log_file(prts->parse_log_f, dirpath, "无法打开素材目录", PARSE_LOG_WARN);
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char sub[256];
        join_path(sub, sizeof(sub), dirpath, entry->d_name);
        if(!path_is_dir(sub)){
            continue;
        }
        if (prts->operator_count >= PRTS_OPERATORS_MAX) {
            parse_log_file(prts->parse_log_f, dirpath, "素材数量超过最大值", PARSE_LOG_WARN);
            break;
        }
        // 干员数组按需扩容（realloc 可能搬家，entry 指针必须每次现取）
        if (prts_operators_reserve(prts, prts->operator_count + 1) != 0) {
            parse_log_file(prts->parse_log_f, dirpath, "干员数组扩容失败", PARSE_LOG_ERROR);
            error_cnt++;
            break;
        }

        join_path(path, sizeof(path), dirpath, entry->d_name);
        if (prts_operator_try_load(prts, &prts->operators[prts->operator_count], path, source, prts->operator_count) == 0) {
            prts->operator_count++;
        }
        else{
            error_cnt ++;
        }
    }
    closedir(dir);
    return error_cnt;
}
#ifndef APP_RELEASE
void prts_operator_log_entry(prts_operator_entry_t* operator){
    log_debug("name: %s", operator->operator_name);
    log_debug("uuid: ");
    uuid_print(&operator->uuid);
    log_trace("description: %s", operator->description);
    log_trace("icon_path: %s", operator->icon_path);
    log_trace("disp_type: %d", operator->disp_type);
    log_trace("intro_video.enabled: %d", operator->intro_video.enabled);
    log_trace("intro_video.duration: %d", operator->intro_video.duration);
    log_trace("intro_video.path: %s", operator->intro_video.path);
    log_trace("loop_video.enabled: %d", operator->loop_video.enabled);
    log_trace("loop_video.duration: %d", operator->loop_video.duration);
    log_trace("loop_video.path: %s", operator->loop_video.path);
    log_trace("opinfo_params.type: %d", operator->opinfo_params.type);
    log_trace("opinfo_params.appear_time: %d", operator->opinfo_params.appear_time);
    log_trace("opinfo_params.duration: %d", operator->opinfo_params.duration);
    log_trace("opinfo_params.operator_name: %s", operator->opinfo_params.operator_name);
    log_trace("opinfo_params.operator_code: %s", operator->opinfo_params.operator_code);
    log_trace("opinfo_params.barcode_text: %s", operator->opinfo_params.barcode_text);
    log_trace("opinfo_params.aux_text: %s", operator->opinfo_params.aux_text);
    log_trace("opinfo_params.staff_text: %s", operator->opinfo_params.staff_text);
    log_trace("opinfo_params.color: %x", operator->opinfo_params.color);
    log_trace("opinfo_params.logo_path: %s", operator->opinfo_params.logo_path);
    log_trace("opinfo_params.class_path: %s", operator->opinfo_params.class_path);
    log_trace("opinfo_params.rhodes_text: %s", operator->opinfo_params.rhodes_text);
    log_trace("opinfo_params.top_right_bar_text: %s", operator->opinfo_params.top_right_bar_text);
    log_trace("opinfo_params.element_count: %d", operator->opinfo_params.element_count);
    log_trace("transition_in.type: %d", operator->transition_in.type);
    log_trace("transition_in.duration: %d", operator->transition_in.duration);
    log_trace("transition_in.background_color: %x", operator->transition_in.background_color);
    log_trace("transition_in.image_path: %s", operator->transition_in.image_path);
    log_trace("transition_loop.type: %d", operator->transition_loop.type);
    log_trace("transition_loop.duration: %d", operator->transition_loop.duration);
    log_trace("transition_loop.background_color: %x", operator->transition_loop.background_color);
    log_trace("transition_loop.image_path: %s", operator->transition_loop.image_path);
}
#endif // APP_RELEASE

