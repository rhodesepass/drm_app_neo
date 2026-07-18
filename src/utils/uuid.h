#pragma once
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    uint8_t data[16];
} uuid_t;


bool uuid_compare(const uuid_t *a, const uuid_t *b);
int uuid_parse(const char *str, uuid_t *uuid);
void uuid_print(const uuid_t *uuid);
// out 至少 37 字节，写入 "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
void uuid_format(const uuid_t *uuid, char *out);