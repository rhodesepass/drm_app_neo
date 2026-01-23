#pragma once

#include "prts/prts.h"

int prts_operator_try_load(prts_t *prts, prts_operator_entry_t* operator, char* path, op_source_t source);
int prts_operator_scan_assets(prts_t *prts, char* dirpath, op_source_t source);

void prts_operator_log_entry(prts_operator_entry_t* operator);