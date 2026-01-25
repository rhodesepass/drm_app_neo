#pragma once

#include "prts/prts.h"

int prts_operator_try_load(prts_t *prts,prts_operator_entry_t* operator,char * path,prts_source_t source,int index);
int prts_operator_scan_assets(prts_t *prts,char* dirpath,prts_source_t source);

#ifndef APP_RELEASE
void prts_operator_log_entry(prts_operator_entry_t* operator);
#else
#define prts_operator_log_entry(operator) do {} while(0)
#endif // APP_RELEASE