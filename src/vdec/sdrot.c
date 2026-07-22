/*
 * cedrus-rotate (VE SDROT) V4L2 m2m post-process 会话。见 sdrot.h。
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "sdrot.h"

#ifndef V4L2_PIX_FMT_SUNXI_TILED_NV12
#define V4L2_PIX_FMT_SUNXI_TILED_NV12 v4l2_fourcc('S', 'T', '1', '2')
#endif

int sdrot_find_device(char *path, unsigned int path_len)
{
	char p[32];
	int i, fd;

	for (i = 0; i < 16; i++) {
		struct v4l2_capability cap;

		snprintf(p, sizeof(p), "/dev/video%d", i);
		fd = open(p, O_RDWR);
		if (fd < 0)
			continue;

		memset(&cap, 0, sizeof(cap));
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
		    strcmp((const char *)cap.card, "cedrus-rotate") == 0) {
			snprintf(path, path_len, "%s", p);
			close(fd);
			return 0;
		}
		close(fd);
	}

	fprintf(stderr, "sdrot: no cedrus-rotate device found\n");
	return -1;
}

static int sdrot_set_ctrl(int fd, unsigned int id, int value)
{
	struct v4l2_control ctrl = { .id = id, .value = value };

	if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
		fprintf(stderr, "sdrot: S_CTRL %08x=%d: %s\n", id, value,
			strerror(errno));
		return -1;
	}
	return 0;
}

static int sdrot_set_fmt(int fd, unsigned int type, unsigned int w,
			 unsigned int h, struct v4l2_format *out)
{
	struct v4l2_format fmt;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SUNXI_TILED_NV12;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		fprintf(stderr, "sdrot: S_FMT type %u: %s\n", type,
			strerror(errno));
		return -1;
	}
	if (out)
		*out = fmt;
	return 0;
}

static int sdrot_reqbufs(int fd, unsigned int type, unsigned int mem,
			 unsigned int count)
{
	struct v4l2_requestbuffers req;

	memset(&req, 0, sizeof(req));
	req.type = type;
	req.memory = mem;
	req.count = count;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		fprintf(stderr, "sdrot: REQBUFS type %u: %s\n", type,
			strerror(errno));
		return -1;
	}
	return 0;
}

static int sdrot_stream(int fd, unsigned int type, bool on)
{
	enum v4l2_buf_type t = type;

	if (ioctl(fd, on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &t) < 0) {
		fprintf(stderr, "sdrot: %s type %u: %s\n",
			on ? "STREAMON" : "STREAMOFF", type, strerror(errno));
		return -1;
	}
	return 0;
}

int sdrot_open(struct sdrot_ctx *s, unsigned int coded_w, unsigned int coded_h,
	       unsigned int in_count, unsigned int out_count,
	       int rotate, int hflip, int vflip)
{
	char path[32];
	struct v4l2_format cfmt;
	unsigned int i;

	memset(s, 0, sizeof(*s));
	s->fd = -1;
	for (i = 0; i < SDROT_MAX_CAP_BUFS; i++)
		s->cap[i].dmabuf_fd = -1;

	if (out_count == 0 || out_count > SDROT_MAX_CAP_BUFS || in_count == 0)
		return -1;

	if (sdrot_find_device(path, sizeof(path)) < 0)
		return -1;

	s->fd = open(path, O_RDWR);
	if (s->fd < 0) {
		fprintf(stderr, "sdrot: open %s: %s\n", path, strerror(errno));
		return -1;
	}

	/*
	 * rotate=0/180 时输入输出同尺寸;90/270 才对调。当前只用 vflip(rotate=0),
	 * 但按通用规则算,方便以后放开旋转。
	 */
	{
		unsigned int cw = coded_w, ch = coded_h;

		if (rotate == 90 || rotate == 270) {
			cw = coded_h;
			ch = coded_w;
		}
		if (sdrot_set_fmt(s->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
				  coded_w, coded_h, NULL) < 0)
			goto error;
		if (sdrot_set_fmt(s->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
				  cw, ch, &cfmt) < 0)
			goto error;
	}

	/* 角度/镜像:任务前设一次即可。驱动会把非法组合(单镜像+90/270)拒掉。 */
	if (sdrot_set_ctrl(s->fd, V4L2_CID_ROTATE, rotate) < 0)
		goto error;
	if (sdrot_set_ctrl(s->fd, V4L2_CID_HFLIP, hflip ? 1 : 0) < 0)
		goto error;
	if (sdrot_set_ctrl(s->fd, V4L2_CID_VFLIP, vflip ? 1 : 0) < 0)
		goto error;

	s->cap_width = cfmt.fmt.pix.width;
	s->cap_height = cfmt.fmt.pix.height;
	s->cap_bytesperline = cfmt.fmt.pix.bytesperline;
	s->cap_sizeimage = cfmt.fmt.pix.sizeimage;
	s->cap_uv_offset = s->cap_bytesperline * s->cap_height;

	if (sdrot_reqbufs(s->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
			  V4L2_MEMORY_DMABUF, in_count) < 0)
		goto error;
	s->in_count = in_count;

	if (sdrot_reqbufs(s->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
			  V4L2_MEMORY_MMAP, out_count) < 0)
		goto error;
	s->cap_count = out_count;

	for (i = 0; i < out_count; i++) {
		struct v4l2_buffer buf;
		struct v4l2_exportbuffer exp;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (ioctl(s->fd, VIDIOC_QUERYBUF, &buf) < 0) {
			fprintf(stderr, "sdrot: QUERYBUF %u: %s\n", i,
				strerror(errno));
			goto error;
		}

		s->cap[i].map = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
				     MAP_SHARED, s->fd, buf.m.offset);
		if (s->cap[i].map == MAP_FAILED) {
			s->cap[i].map = NULL;
			fprintf(stderr, "sdrot: mmap cap %u: %s\n", i,
				strerror(errno));
			goto error;
		}
		s->cap[i].length = buf.length;

		memset(&exp, 0, sizeof(exp));
		exp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		exp.index = i;
		exp.flags = O_RDONLY;
		if (ioctl(s->fd, VIDIOC_EXPBUF, &exp) < 0) {
			fprintf(stderr, "sdrot: EXPBUF cap %u: %s\n", i,
				strerror(errno));
			goto error;
		}
		s->cap[i].dmabuf_fd = exp.fd;
	}

	if (sdrot_stream(s->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, true) < 0)
		goto error;
	if (sdrot_stream(s->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, true) < 0)
		goto error;
	s->streaming = true;

	return 0;

error:
	sdrot_close(s);
	return -1;
}

int sdrot_process(struct sdrot_ctx *s, int in_dmabuf_fd, unsigned int in_index,
		  unsigned int out_index)
{
	struct v4l2_buffer buf;
	bool src_err = false, dst_err = false;

	if (in_index >= s->in_count || out_index >= s->cap_count)
		return -1;

	/* 输入:DMABUF,喂解码那帧 */
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = in_index;
	buf.m.fd = in_dmabuf_fd;
	buf.bytesused = s->cap_sizeimage;
	if (ioctl(s->fd, VIDIOC_QBUF, &buf) < 0) {
		fprintf(stderr, "sdrot: QBUF out: %s\n", strerror(errno));
		return -1;
	}

	/* 输出:MMAP,翻转后的帧落到 cap[out_index] */
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = out_index;
	if (ioctl(s->fd, VIDIOC_QBUF, &buf) < 0) {
		fprintf(stderr, "sdrot: QBUF cap: %s\n", strerror(errno));
		return -1;
	}

	/* 阻塞 DQBUF 两端(fd 阻塞打开) */
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (ioctl(s->fd, VIDIOC_DQBUF, &buf) < 0) {
		fprintf(stderr, "sdrot: DQBUF cap: %s\n", strerror(errno));
		return -1;
	}
	dst_err = !!(buf.flags & V4L2_BUF_FLAG_ERROR);

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_DMABUF;
	if (ioctl(s->fd, VIDIOC_DQBUF, &buf) < 0) {
		fprintf(stderr, "sdrot: DQBUF out: %s\n", strerror(errno));
		return -1;
	}
	src_err = !!(buf.flags & V4L2_BUF_FLAG_ERROR);

	if (src_err || dst_err) {
		fprintf(stderr, "sdrot: rotate error%s%s\n",
			src_err ? " src" : "", dst_err ? " dst" : "");
		return -1;
	}

	return 0;
}

void sdrot_close(struct sdrot_ctx *s)
{
	unsigned int i;

	if (s->fd < 0)
		return;

	if (s->streaming) {
		sdrot_stream(s->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, false);
		sdrot_stream(s->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, false);
		s->streaming = false;
	}

	for (i = 0; i < s->cap_count; i++) {
		if (s->cap[i].map)
			munmap(s->cap[i].map, s->cap[i].length);
		if (s->cap[i].dmabuf_fd >= 0)
			close(s->cap[i].dmabuf_fd);
	}

	/* 释放 vb2 队列,让 CMA 归还 */
	sdrot_reqbufs(s->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_MEMORY_DMABUF, 0);
	sdrot_reqbufs(s->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP, 0);
	close(s->fd);

	memset(s, 0, sizeof(*s));
	s->fd = -1;
}
