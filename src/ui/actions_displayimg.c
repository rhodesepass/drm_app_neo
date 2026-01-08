// 扩列图展示 专用
#include "ui.h"
#include "ui/actions_displayimg.h"
#include "utils/log.h"

// =========================================
// 自己添加的方法 START
// =========================================

// =========================================
// EEZ 回调 START
// =========================================

const char *get_var_displayimg_size_lbl(){
    return "1/2";
}
void set_var_displayimg_size_lbl(const char *value){
    return;
}

void action_displayimg_key(lv_event_t * e){
    log_debug("action_displayimg_key");
}