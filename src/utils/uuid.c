#include "utils/uuid.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "utils/log.h"

bool uuid_compare(const uuid_t *a, const uuid_t *b){
    return memcmp(a->data, b->data, 16) == 0;
}
int uuid_parse(const char *str, uuid_t *uuid){
    if(strlen(str) != 36){
        return -1;
    }
    if(str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-'){
        return -1;
    }
    // Parse a UUID string in the form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    int i = 0, j = 0;
    for (i = 0, j = 0; i < 36 && j < 16; ) {
        if (str[i] == '-') {
            i++;
            continue;
        }
        int hi, lo;
        char c1 = str[i];
        char c2 = str[i+1];
        if ('0' <= c1 && c1 <= '9') hi = c1 - '0';
        else if ('a' <= c1 && c1 <= 'f') hi = c1 - 'a' + 10;
        else if ('A' <= c1 && c1 <= 'F') hi = c1 - 'A' + 10;
        else return -1;
        if ('0' <= c2 && c2 <= '9') lo = c2 - '0';
        else if ('a' <= c2 && c2 <= 'f') lo = c2 - 'a' + 10;
        else if ('A' <= c2 && c2 <= 'F') lo = c2 - 'A' + 10;
        else return -1;
        uuid->data[j++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }
    if (j != 16) return -1;
    return 0;
}
void uuid_format(const uuid_t *uuid, char *out){
    snprintf(out, 37,
        "%02x%02x%02x%02x-"
        "%02x%02x-"
        "%02x%02x-"
        "%02x%02x-"
        "%02x%02x%02x%02x%02x%02x",
        uuid->data[0], uuid->data[1], uuid->data[2], uuid->data[3],
        uuid->data[4], uuid->data[5],
        uuid->data[6], uuid->data[7],
        uuid->data[8], uuid->data[9],
        uuid->data[10], uuid->data[11], uuid->data[12], uuid->data[13], uuid->data[14], uuid->data[15]
    );
}
void uuid_print(const uuid_t *uuid){
    // Print the UUID in the form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    // Returns number of chars printed
    log_debug(
        "%02x%02x%02x%02x-"
        "%02x%02x-"
        "%02x%02x-"
        "%02x%02x-"
        "%02x%02x%02x%02x%02x%02x",
        uuid->data[0], uuid->data[1], uuid->data[2], uuid->data[3],
        uuid->data[4], uuid->data[5],
        uuid->data[6], uuid->data[7],
        uuid->data[8], uuid->data[9],
        uuid->data[10], uuid->data[11], uuid->data[12], uuid->data[13], uuid->data[14], uuid->data[15]
    );
}