/*
 * cedrus-rotate (VE SDROT) V4L2 m2m post-process：对解码出来的 tiled NV12
 * 整帧做硬件旋转 / 镜像。boe 等倒装机型靠它给视频层内容补 Y 翻转
 * (V4L2_CID_VFLIP)——DEBE 只翻层坐标和 BE 直灌的层，不翻 frontend 灌进来的
 * 视频内容，见 docs/boe-flip-180.md 与 cedrus-rotate-usage.md。
 *
 * 输入端零拷贝：直接吃解码 CAPTURE 导出的 dmabuf(与解码同为 ST12,布局一致)。
 * 输出端自带一个小 capture 池,EXPBUF 出去当显示 FB。一次任务同步完成
 * (QBUF 两端 → 阻塞 DQBUF),和解码串行共用 VE。
 */

#ifndef _VDEC_SDROT_H_
#define _VDEC_SDROT_H_

#include <stdbool.h>
#include <stdint.h>

#define SDROT_MAX_CAP_BUFS	12	/* 显示保持 2 + 平滑储备上限 */

struct sdrot_cap_buf {
	void *map;
	unsigned int length;
	int dmabuf_fd;
};

struct sdrot_ctx {
	int fd;
	bool streaming;

	/* 输入端：DMABUF,index == 解码 CAPTURE slot,布局与解码一致 */
	unsigned int in_count;

	/* 输出端(翻转后):驱动回读布局,与解码同为 tiled NV12 */
	unsigned int cap_count;
	unsigned int cap_width;
	unsigned int cap_height;
	unsigned int cap_bytesperline;
	unsigned int cap_uv_offset;	/* luma 平面大小 */
	unsigned int cap_sizeimage;
	struct sdrot_cap_buf cap[SDROT_MAX_CAP_BUFS];
};

/* 遍历 /dev/video*,认 cap.card == "cedrus-rotate"。0 成功,path 回填。 */
int sdrot_find_device(char *path, unsigned int path_len);

/*
 * 打开并配置到 STREAMON。coded_w/coded_h 传解码用的编码尺寸(MB 对齐)——
 * 与解码 CAPTURE 的实际布局一致,输入 dmabuf 的平面偏移/长度才对得上。
 * rotate ∈ {0,90,180,270},vflip/hflip ∈ {0,1}(约束见 cedrus-rotate-usage.md)。
 * in_count = 解码 CAPTURE buffer 数(输入端 DMABUF slot 数,按 index 缓存挂载)。
 * out_count = 翻转输出池深度(= 显示保持格数)。
 */
int sdrot_open(struct sdrot_ctx *s, unsigned int coded_w, unsigned int coded_h,
	       unsigned int in_count, unsigned int out_count,
	       int rotate, int hflip, int vflip);

/*
 * 转一帧:输入 dmabuf(in_dmabuf_fd,挂在 in_index 这个 DMABUF slot 上)→
 * 输出到 cap[out_index]。同步,阻塞到硬件转完。返回 0 成功。
 */
int sdrot_process(struct sdrot_ctx *s, int in_dmabuf_fd, unsigned int in_index,
		  unsigned int out_index);

void sdrot_close(struct sdrot_ctx *s);

#endif /* _VDEC_SDROT_H_ */
