/*
 * Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <linux/media.h>
#include <linux/videodev2.h>

#include "vdec_v4l2.h"

static int query_capabilities(int video_fd, unsigned int *capabilities)
{
	struct v4l2_capability capability;
	int rc;

	memset(&capability, 0, sizeof(capability));

	rc = ioctl(video_fd, VIDIOC_QUERYCAP, &capability);
	if (rc < 0)
		return -1;

	if ((capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0)
		*capabilities = capability.device_caps;
	else
		*capabilities = capability.capabilities;

	return 0;
}

static int set_format(int video_fd, unsigned int type, unsigned int width,
		      unsigned int height, unsigned int pixelformat,
		      unsigned int sizeimage)
{
	struct v4l2_format format;
	int rc;

	memset(&format, 0, sizeof(format));
	format.type = type;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.sizeimage = sizeimage;
	format.fmt.pix.pixelformat = pixelformat;

	rc = ioctl(video_fd, VIDIOC_S_FMT, &format);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to set format for type %d: %s\n",
			type, strerror(errno));
		return -1;
	}

	return 0;
}

static int create_buffers(int video_fd, unsigned int type,
			  unsigned int buffers_count)
{
	struct v4l2_create_buffers buffers;
	int rc;

	memset(&buffers, 0, sizeof(buffers));
	buffers.format.type = type;
	buffers.memory = V4L2_MEMORY_MMAP;
	buffers.count = buffers_count;

	rc = ioctl(video_fd, VIDIOC_G_FMT, &buffers.format);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to get format for type %d: %s\n",
			type, strerror(errno));
		return -1;
	}

	rc = ioctl(video_fd, VIDIOC_CREATE_BUFS, &buffers);
	if (rc < 0) {
		fprintf(stderr,
			"vdec: unable to create buffers for type %d: %s\n",
			type, strerror(errno));
		return -1;
	}

	return 0;
}

static int request_buffers(int video_fd, unsigned int type,
			   unsigned int buffers_count)
{
	struct v4l2_requestbuffers buffers;
	int rc;

	memset(&buffers, 0, sizeof(buffers));
	buffers.type = type;
	buffers.memory = V4L2_MEMORY_MMAP;
	buffers.count = buffers_count;

	rc = ioctl(video_fd, VIDIOC_REQBUFS, &buffers);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to request buffers: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

static int query_and_map(int video_fd, unsigned int type, unsigned int index,
			 void **map, unsigned int *length)
{
	struct v4l2_buffer buffer;
	int rc;

	memset(&buffer, 0, sizeof(buffer));
	buffer.type = type;
	buffer.memory = V4L2_MEMORY_MMAP;
	buffer.index = index;

	rc = ioctl(video_fd, VIDIOC_QUERYBUF, &buffer);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to query buffer: %s\n",
			strerror(errno));
		return -1;
	}

	*map = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
		    video_fd, buffer.m.offset);
	if (*map == MAP_FAILED) {
		fprintf(stderr, "vdec: unable to map buffer: %s\n",
			strerror(errno));
		return -1;
	}

	*length = buffer.length;
	return 0;
}

static int queue_buffer(int video_fd, int request_fd, unsigned int type,
			uint64_t ts, unsigned int index, unsigned int size)
{
	struct v4l2_buffer buffer;
	int rc;

	memset(&buffer, 0, sizeof(buffer));
	buffer.type = type;
	buffer.memory = V4L2_MEMORY_MMAP;
	buffer.index = index;
	buffer.bytesused = size;

	if (request_fd >= 0) {
		buffer.flags = V4L2_BUF_FLAG_REQUEST_FD;
		buffer.request_fd = request_fd;
	}

	/*
	 * 驱动用 v4l2_timeval_to_ns() 还原后按 ns 精确匹配 reference_ts，
	 * 必须无损换算（t113 原版 tv_usec = ts/1000 在 ts>=1e9 后回转失败）。
	 */
	buffer.timestamp.tv_sec = ts / 1000000000ULL;
	buffer.timestamp.tv_usec = (ts % 1000000000ULL) / 1000;

	rc = ioctl(video_fd, VIDIOC_QBUF, &buffer);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to queue buffer type %d: %s\n",
			type, strerror(errno));
		return -1;
	}

	return 0;
}

static int dequeue_buffer(int video_fd, unsigned int type, bool *error)
{
	struct v4l2_buffer buffer;
	int rc;

	memset(&buffer, 0, sizeof(buffer));
	buffer.type = type;
	buffer.memory = V4L2_MEMORY_MMAP;

	rc = ioctl(video_fd, VIDIOC_DQBUF, &buffer);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to dequeue buffer type %d: %s\n",
			type, strerror(errno));
		return -1;
	}

	if (error != NULL)
		*error = !!(buffer.flags & V4L2_BUF_FLAG_ERROR);

	return buffer.index;
}

static int set_control(int video_fd, int request_fd, unsigned int id,
		       void *data, unsigned int size)
{
	struct v4l2_ext_control control;
	struct v4l2_ext_controls controls;
	int rc;

	memset(&control, 0, sizeof(control));
	memset(&controls, 0, sizeof(controls));

	control.id = id;
	control.ptr = data;
	control.size = size;

	controls.controls = &control;
	controls.count = 1;

	if (request_fd >= 0) {
		controls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
		controls.request_fd = request_fd;
	}

	rc = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &controls);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to set control %08x: %s\n", id,
			strerror(errno));
		return -1;
	}

	return 0;
}

static int set_stream(int video_fd, unsigned int type, bool enable)
{
	enum v4l2_buf_type buf_type = type;
	int rc;

	rc = ioctl(video_fd, enable ? VIDIOC_STREAMON : VIDIOC_STREAMOFF,
		   &buf_type);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to %s stream type %d: %s\n",
			enable ? "start" : "stop", type, strerror(errno));
		return -1;
	}

	return 0;
}

static int set_frame_controls(int video_fd, int request_fd,
			      struct vdec_h264_ctrls *c)
{
	static const struct {
		unsigned int id;
		unsigned int offset;
		unsigned int size;
	} glue[] = {
		{ V4L2_CID_MPEG_VIDEO_H264_DECODE_PARAMS,
		  offsetof(struct vdec_h264_ctrls, decode_params),
		  sizeof(c->decode_params) },
		{ V4L2_CID_MPEG_VIDEO_H264_PPS,
		  offsetof(struct vdec_h264_ctrls, pps), sizeof(c->pps) },
		{ V4L2_CID_MPEG_VIDEO_H264_SPS,
		  offsetof(struct vdec_h264_ctrls, sps), sizeof(c->sps) },
		{ V4L2_CID_MPEG_VIDEO_H264_SCALING_MATRIX,
		  offsetof(struct vdec_h264_ctrls, scaling_matrix),
		  sizeof(c->scaling_matrix) },
		{ V4L2_CID_MPEG_VIDEO_H264_SLICE_PARAMS,
		  offsetof(struct vdec_h264_ctrls, slice_params),
		  sizeof(c->slice_params) },
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(glue); i++)
		if (set_control(video_fd, request_fd, glue[i].id,
				(uint8_t *)c + glue[i].offset,
				glue[i].size) < 0)
			return -1;

	return 0;
}

int vdec_find_device(char *video_path, unsigned int video_path_len,
		     char *media_path, unsigned int media_path_len)
{
	char path[32];
	int i, fd;
	bool found = false;

	for (i = 0; i < 16 && !found; i++) {
		struct v4l2_capability capability;

		snprintf(path, sizeof(path), "/dev/video%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			continue;

		memset(&capability, 0, sizeof(capability));
		if (ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0 &&
		    strcmp((const char *)capability.driver, "cedrus") == 0) {
			snprintf(video_path, video_path_len, "%s", path);
			found = true;
		}
		close(fd);
	}
	if (!found) {
		fprintf(stderr, "vdec: no cedrus video device found\n");
		return -1;
	}

	found = false;
	for (i = 0; i < 16 && !found; i++) {
		struct media_device_info info;

		snprintf(path, sizeof(path), "/dev/media%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			continue;

		memset(&info, 0, sizeof(info));
		if (ioctl(fd, MEDIA_IOC_DEVICE_INFO, &info) == 0 &&
		    strcmp(info.driver, "cedrus") == 0) {
			snprintf(media_path, media_path_len, "%s", path);
			found = true;
		}
		close(fd);
	}
	if (!found) {
		fprintf(stderr, "vdec: no cedrus media device found\n");
		return -1;
	}

	return 0;
}

int vdec_open(struct vdec_ctx *v, const char *video_path,
	      const char *media_path, unsigned int width, unsigned int height,
	      unsigned int cap_count, unsigned int out_count,
	      unsigned int out_size)
{
	struct v4l2_format format;
	unsigned int capabilities;
	unsigned int i;
	int rc;

	memset(v, 0, sizeof(*v));
	v->video_fd = -1;
	v->media_fd = -1;
	for (i = 0; i < VDEC_MAX_CAP_BUFS; i++)
		v->cap[i].dmabuf_fd = -1;
	for (i = 0; i < VDEC_MAX_OUT_BUFS; i++)
		v->out[i].request_fd = -1;

	if (cap_count > VDEC_MAX_CAP_BUFS || out_count > VDEC_MAX_OUT_BUFS ||
	    cap_count == 0 || out_count == 0)
		return -1;

	v->video_fd = open(video_path, O_RDWR | O_NONBLOCK);
	if (v->video_fd < 0) {
		fprintf(stderr, "vdec: unable to open %s: %s\n", video_path,
			strerror(errno));
		goto error;
	}

	v->media_fd = open(media_path, O_RDWR | O_NONBLOCK);
	if (v->media_fd < 0) {
		fprintf(stderr, "vdec: unable to open %s: %s\n", media_path,
			strerror(errno));
		goto error;
	}

	if (query_capabilities(v->video_fd, &capabilities) < 0 ||
	    (capabilities & (V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M)) !=
		    (V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M)) {
		fprintf(stderr, "vdec: missing required capabilities\n");
		goto error;
	}

	rc = set_format(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, width, height,
			V4L2_PIX_FMT_H264_SLICE, out_size);
	if (rc < 0)
		goto error;

	rc = set_format(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, width,
			height, V4L2_PIX_FMT_SUNXI_TILED_NV12, 0);
	if (rc < 0)
		goto error;

	/* cedrus 的默认值就是这两个，但显式设置以明确契约 */
	{
		struct v4l2_ext_control ctrls[2];
		struct v4l2_ext_controls ext;

		memset(ctrls, 0, sizeof(ctrls));
		memset(&ext, 0, sizeof(ext));

		ctrls[0].id = V4L2_CID_MPEG_VIDEO_H264_DECODE_MODE;
		ctrls[0].value = V4L2_MPEG_VIDEO_H264_DECODE_MODE_SLICE_BASED;
		ctrls[1].id = V4L2_CID_MPEG_VIDEO_H264_START_CODE;
		ctrls[1].value = V4L2_MPEG_VIDEO_H264_START_CODE_NONE;

		ext.controls = ctrls;
		ext.count = 2;

		rc = ioctl(v->video_fd, VIDIOC_S_EXT_CTRLS, &ext);
		if (rc < 0) {
			fprintf(stderr,
				"vdec: unable to set decode mode/start code: %s\n",
				strerror(errno));
			goto error;
		}
	}

	/* CAPTURE 实际布局（tiled NV12: bpl=ALIGN(w,32), h=ALIGN(h,32)） */
	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rc = ioctl(v->video_fd, VIDIOC_G_FMT, &format);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to get capture format: %s\n",
			strerror(errno));
		goto error;
	}
	v->cap_width = format.fmt.pix.width;
	v->cap_height = format.fmt.pix.height;
	v->cap_bytesperline = format.fmt.pix.bytesperline;
	v->cap_sizeimage = format.fmt.pix.sizeimage;
	v->cap_uv_offset = v->cap_bytesperline * v->cap_height;

	rc = create_buffers(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, out_count);
	if (rc < 0)
		goto error;
	v->out_count = out_count;

	for (i = 0; i < out_count; i++) {
		rc = query_and_map(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, i,
				   &v->out[i].map, &v->out[i].length);
		if (rc < 0)
			goto error;

		rc = ioctl(v->media_fd, MEDIA_IOC_REQUEST_ALLOC,
			   &v->out[i].request_fd);
		if (rc < 0) {
			fprintf(stderr,
				"vdec: unable to allocate media request: %s\n",
				strerror(errno));
			goto error;
		}
	}

	rc = create_buffers(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
			    cap_count);
	if (rc < 0)
		goto error;
	v->cap_count = cap_count;

	for (i = 0; i < cap_count; i++) {
		struct v4l2_exportbuffer exportbuffer;

		rc = query_and_map(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i,
				   &v->cap[i].map, &v->cap[i].length);
		if (rc < 0)
			goto error;

		memset(&exportbuffer, 0, sizeof(exportbuffer));
		exportbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		exportbuffer.index = i;
		exportbuffer.flags = O_RDONLY;

		rc = ioctl(v->video_fd, VIDIOC_EXPBUF, &exportbuffer);
		if (rc < 0) {
			fprintf(stderr,
				"vdec: unable to export capture buffer: %s\n",
				strerror(errno));
			goto error;
		}
		v->cap[i].dmabuf_fd = exportbuffer.fd;
	}

	rc = set_stream(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, true);
	if (rc < 0)
		goto error;

	rc = set_stream(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, true);
	if (rc < 0)
		goto error;

	return 0;

error:
	vdec_close(v);
	return -1;
}

void vdec_close(struct vdec_ctx *v)
{
	unsigned int i;

	if (v->video_fd >= 0) {
		set_stream(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, false);
		set_stream(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, false);
	}

	for (i = 0; i < v->out_count; i++) {
		if (v->out[i].map)
			munmap(v->out[i].map, v->out[i].length);
		if (v->out[i].request_fd >= 0)
			close(v->out[i].request_fd);
	}

	for (i = 0; i < v->cap_count; i++) {
		if (v->cap[i].map)
			munmap(v->cap[i].map, v->cap[i].length);
		if (v->cap[i].dmabuf_fd >= 0)
			close(v->cap[i].dmabuf_fd);
	}

	if (v->video_fd >= 0) {
		/* 释放 vb2 队列，让 CMA 归还 */
		request_buffers(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0);
		request_buffers(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0);
		close(v->video_fd);
	}
	if (v->media_fd >= 0)
		close(v->media_fd);

	memset(v, 0, sizeof(*v));
	v->video_fd = -1;
	v->media_fd = -1;
}

static int64_t vdec_now_us(void)
{
	struct timespec tsp;

	clock_gettime(CLOCK_MONOTONIC, &tsp);
	return (int64_t)tsp.tv_sec * 1000000 + tsp.tv_nsec / 1000;
}

int vdec_decode(struct vdec_ctx *v, int cap_slot, uint64_t ts,
		const uint8_t *nal, unsigned int nal_size,
		struct vdec_h264_ctrls *ctrls)
{
	struct timeval tv = { 0, 300000 };
	unsigned int out_index = v->out_next;
	int request_fd = v->out[out_index].request_fd;
	fd_set except_fds;
	bool src_error = false, dst_error = false;
	int rc;

	if (cap_slot < 0 || (unsigned int)cap_slot >= v->cap_count)
		return -1;
	if (nal_size > v->out[out_index].length) {
		fprintf(stderr, "vdec: NAL too large (%u > %u)\n", nal_size,
			v->out[out_index].length);
		return -1;
	}

	v->out_next = (out_index + 1) % v->out_count;

	memcpy(v->out[out_index].map, nal, nal_size);
	ctrls->slice_params.size = nal_size;
	ctrls->slice_params.start_byte_offset = 0;

	int64_t t0 = vdec_now_us(), t1, t2, t3, t4;

	rc = set_frame_controls(v->video_fd, request_fd, ctrls);
	if (rc < 0)
		return -1;

	t1 = vdec_now_us();

	rc = queue_buffer(v->video_fd, request_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
			  ts, out_index, nal_size);
	if (rc < 0)
		goto error_reinit;

	rc = queue_buffer(v->video_fd, -1, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			  cap_slot, 0);
	if (rc < 0)
		goto error_reinit;

	t2 = vdec_now_us();

	rc = ioctl(request_fd, MEDIA_REQUEST_IOC_QUEUE, NULL);
	if (rc < 0) {
		fprintf(stderr, "vdec: unable to queue media request: %s\n",
			strerror(errno));
		goto error_reinit;
	}

	t3 = vdec_now_us();

	FD_ZERO(&except_fds);
	FD_SET(request_fd, &except_fds);

	rc = select(request_fd + 1, NULL, NULL, &except_fds, &tv);
	if (rc <= 0) {
		fprintf(stderr, "vdec: %s waiting for media request\n",
			rc == 0 ? "timeout" : "error");
		return -1;
	}

	t4 = vdec_now_us();
	if (t4 - t0 > (v->slow_threshold_us ?
		       v->slow_threshold_us : 50000))
		fprintf(stderr, "vdec: slow breakdown ctrl=%lld qbuf=%lld reqq=%lld wait=%lld us\n",
			(long long)(t1 - t0), (long long)(t2 - t1),
			(long long)(t3 - t2), (long long)(t4 - t3));

	rc = dequeue_buffer(v->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
			    &src_error);
	if (rc < 0)
		return -1;

	rc = dequeue_buffer(v->video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
			    &dst_error);
	if (rc < 0)
		return -1;

	/*
	 * 失败也要 REINIT，否则 request 保持 dirty，下次复用这个 slot 的
	 * S_EXT_CTRLS 会 EBUSY。
	 */
	if (ioctl(request_fd, MEDIA_REQUEST_IOC_REINIT, NULL) < 0) {
		fprintf(stderr, "vdec: unable to reinit media request: %s\n",
			strerror(errno));
		return -1;
	}

	if (src_error || dst_error) {
		fprintf(stderr, "vdec: decode error (ts %llu)%s%s\n",
			(unsigned long long)ts, src_error ? " src" : "",
			dst_error ? " dst" : "");
		return -1;
	}

	return 0;

error_reinit:
	/*
	 * 半路失败也要 REINIT：request 里已装了控制(可能还有 OUTPUT buffer)，
	 * 不清掉的话复用该 slot 时 S_EXT_CTRLS 返回 EBUSY，close 时内核还会
	 * 对未 queue 的 dirty request 打 WARN。
	 */
	ioctl(request_fd, MEDIA_REQUEST_IOC_REINIT, NULL);
	return -1;
}
