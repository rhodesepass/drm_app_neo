// 设备信息页面 专用
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 自己添加的方法
void ui_sysinfo_get_meminfo_str(char *ret, size_t ret_sz);
void ui_sysinfo_get_os_release_str(char *ret, size_t ret_sz);
uint32_t ui_sysinfo_get_file_crc32(const char *path);
bool ui_sysinfo_is_sdcard_inserted();
uint64_t ui_sysinfo_get_nand_available_size();
uint64_t ui_sysinfo_get_sd_available_size();
uint64_t ui_sysinfo_get_nand_total_size();
uint64_t ui_sysinfo_get_sd_total_size();
int ui_sysinfo_format_sd_card();
// EEZ回调不需要添加。