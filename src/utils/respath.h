#pragma once

// 资源目录在运行时按"可执行文件同级的 RES_SUBDIR 子目录"解析 (config.h 定义 RES_SUBDIR)。
// 设备侧不再依赖 /root/res 这种绝对路径; sim 仍可用 -DFONT_REGISTRY_DIR / -DUI_IMG_DIR 覆盖。
//
// respath_init 必须在使用任何路径前调用一次 (失败时回退到编译期兜底)。
// respath / respath_lvfs 返回内部轮转的静态缓冲, 调用方用完即取 (单线程 init 场景足够,
// 同一表达式里别嵌套超过 4 个调用)。respath_lvfs 额外加 lv_fs 'A:' 盘符。

void respath_init(void);
const char *respath_dir(void);             // 资源根目录, 无尾斜杠
const char *respath(const char *rel);      // <res>/<rel>
const char *respath_lvfs(const char *rel); // A:<res>/<rel>
