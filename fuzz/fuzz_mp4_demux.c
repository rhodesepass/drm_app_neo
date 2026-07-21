/*
 * mp4_demux fuzz target。mp4_open 解析头时 mmap 文件，之后 sample 数据走
 * pread，所以先把 libFuzzer 喂进来的字节落到临时文件，再模拟 mediaplayer
 * 的实际调用序列：开容器 -> 遍历所有 sample(pread 进复用缓冲) -> 关闭。
 */
#include "vdec/mp4_demux.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char path[] = "/tmp/epass_fuzz_mp4_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0)
		return 0;
	if (write(fd, data, size) != (ssize_t)size) {
		close(fd);
		unlink(path);
		return 0;
	}
	close(fd);

	struct mp4_demux m;
	memset(&m, 0, sizeof(m));
	if (mp4_open(&m, path) == 0) {
		for (unsigned int i = 0; i < m.samples_count; i++) {
			const uint8_t *au = NULL;
			uint32_t sz = 0;
			/* pread 进复用缓冲；触碰首尾字节逼 ASan 检出越界 */
			if (mp4_read_sample(&m, i, &au, &sz) == MP4_OK && au && sz) {
				volatile uint8_t t = au[0];
				t ^= au[sz - 1];
				(void)t;
			}
		}
		mp4_close(&m);
	}

	unlink(path);
	return 0;
}
