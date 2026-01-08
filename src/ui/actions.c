// UI 公用事件 实现

#include <stdint.h>
#include <stdlib.h>

#include "actions.h"
#include "screens.h"
#include "vars.h"
#include "utils/log.h"
#include "render/layer_animation.h"
#include "config.h"
#include "render/lvgl_drm_warp.h"
#include "ui.h"
#include "utils/settings.h"
#include "utils/timer.h"
#include "ui/filemanager.h"

extern settings_t g_settings;


void action_op_sel_cb(lv_event_t * e){
    log_debug("action_op_sel_cb");
}
