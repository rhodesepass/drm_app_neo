#include "apps/extmap.h"
#include "utils/log.h"


static int ext_lower_with_check(char* dest,char* src,int max_len){
    for(int i = 0; i < max_len; i++){
        dest[i] = src[i];
        // to lowercase
        if(dest[i] >= 'A' && dest[i] <= 'Z'){
            dest[i] += 'a' - 'A';
        }
        else if((dest[i] >= 'a' && dest[i] <= 'z') || dest[i] == '.'){
            continue;
        }
        else if(dest[i] == '\0'){
            break;
        }
        else{
            return -1;
        }
    }
    dest[max_len - 1] = '\0';
    return 0;
}

int apps_extmap_init(apps_extmap_t *extmap){
    extmap->extmap_count = 0;
    return 0;
}

int apps_extmap_destroy(apps_extmap_t *extmap){
    extmap->extmap_count = 0;
    return 0;
}

int apps_extmap_add(apps_extmap_t *extmap, char *ext, app_entry_t *app){
    if(extmap->extmap_count >= APPS_EXTMAP_MAX){
        log_error("extmap is full");
        return -1;
    }

    char buf[10];

    if(ext_lower_with_check(buf, ext, sizeof(buf)) < 0){
        log_error("add:invalid character in ext: %s", ext);
        return -1;
    }

    for(int i = 0; i < extmap->extmap_count; i++){
        if(strcmp(extmap->extmap[i].ext, buf) == 0){
            log_error("add:ext already exists: %s", ext);
            return -1;
        }
    }

    strncpy(extmap->extmap[extmap->extmap_count].ext, buf, sizeof(extmap->extmap[extmap->extmap_count].ext));
    extmap->extmap[extmap->extmap_count].app = app;
    extmap->extmap_count++;
    return 0;
}

int apps_extmap_get(apps_extmap_t *extmap, char *ext, app_entry_t **app){
    char buf[10];
    if(ext_lower_with_check(buf, ext, sizeof(buf)) < 0){
        log_error("get:invalid character in ext: %s", ext);
        return -1;
    }
    for(int i = 0; i < extmap->extmap_count; i++){
        if(strcmp(extmap->extmap[i].ext, buf) == 0){
            *app = extmap->extmap[i].app;
            return 0;
        }
    }
    log_error("get:ext not found: %s", ext);
    return -1;
}

#ifndef APP_RELEASE
void apps_extmap_log_entry(apps_extmap_t *extmap){
    log_debug("extmap_count: %d", extmap->extmap_count);
    for(int i = 0; i < extmap->extmap_count; i++){
        log_debug("ext: %s", extmap->extmap[i].ext);
        log_debug("app: %s", extmap->extmap[i].app->app_name);
    }
}
#endif // APP_RELEASE