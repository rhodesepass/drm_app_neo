/*
 * epconfig.json fuzz target。目标是 prts_operator_try_load 的字段校验逻辑:
 * 非法/缺失/畸形字段应当写 parse_log 并干净返回 -1, 而不是崩溃/越界/UB。
 *
 * try_load 读的是 <op_dir>/epconfig.json, 所以把 libFuzzer 的字节写进一个
 * 临时目录下的 epconfig.json, 再以该目录为 op_dir 调用。file_exists_readable
 * 对 loop.file/icon 等一律返回假(临时目录里没有这些文件), 校验会在"文件不存在"
 * 处早退 —— 这正是我们要覆盖的错误路径之一, 且不触碰真实解码/GUI。
 *
 * GUI/资源叶子函数(respath_lvfs / overlay_opinfo_free_elements)在本文件打桩,
 * 其余(cJSON / misc / uuid)链接真实实现。
 */
#include "prts/prts.h"
#include "prts/operators.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- 打桩: 顶掉会拖入 lvgl/opinfo 的叶子函数 ---- */
const char *respath_lvfs(const char *rel)
{
	static char buf[256];
	snprintf(buf, sizeof(buf), "A:res/%s", rel ? rel : "");
	return buf;
}

void overlay_opinfo_free_elements(olopinfo_params_t *params)
{
	(void)params;	/* 解析产物由 harness 每轮 memset 覆盖, 无需真释放 */
}

void overlay_opinfo_element_init(olopinfo_element_t *el)
{
	if (el)
		memset(el, 0, sizeof(*el));
}

int overlay_opinfo_build_image_elements(olopinfo_params_t *params)
{
	(void)params;	/* 建 image 元素要读真实图片资源, fuzz 不关心, 直接放行 */
	return 0;
}

int prts_operators_reserve(prts_t *prts, int need)
{
	(void)prts;
	(void)need;
	return 0;	/* try_load 路径不触发扩容; scan_assets 才用, 桩返回成功 */
}

void log_log(int level, const char *file, int line, const char *fmt, ...)
{
	(void)level; (void)file; (void)line; (void)fmt;
}

/* ---- harness ---- */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char dir[] = "/tmp/epass_fuzz_epc_XXXXXX";
	if (!mkdtemp(dir))
		return 0;

	char json_path[300];
	snprintf(json_path, sizeof(json_path), "%s/epconfig.json", dir);
	FILE *f = fopen(json_path, "wb");
	if (!f)
		goto cleanup;
	if (size)
		fwrite(data, 1, size, f);
	fclose(f);

	prts_t prts;
	memset(&prts, 0, sizeof(prts));
	prts.parse_log_f = NULL;	/* parse_log_file 需容忍 NULL FILE* */

	prts_operator_entry_t op;
	memset(&op, 0, sizeof(op));

	if (prts_operator_try_load(&prts, &op, dir, PRTS_SOURCE_NAND, 0) == 0)
		prts_operator_entry_free(&op);

cleanup:
	unlink(json_path);
	rmdir(dir);
	return 0;
}
