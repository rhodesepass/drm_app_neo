#pragma once

#include "prts/prts.h"

int prts_operator_try_load(prts_t *prts,prts_operator_entry_t* operator,char * path,prts_source_t source,int index);
int prts_operator_scan_assets(prts_t *prts,char* dirpath,prts_source_t source);

// 释放 entry 持有的堆资源（opinfo 元素列表及其中已加载的图片）。
// 重新解析(reload)或销毁前必须调用，否则泄漏。
void prts_operator_entry_free(prts_operator_entry_t* operator);

#ifndef APP_RELEASE
void prts_operator_log_entry(prts_operator_entry_t* operator);
#else
#define prts_operator_log_entry(operator) do {} while(0)
#endif // APP_RELEASE