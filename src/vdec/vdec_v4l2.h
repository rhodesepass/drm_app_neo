/*
 * V4L2 stateless H.264 decode engine (request API) for cedrus on kernel 5.4.
 *
 * Derived from v4l2-request-test (GPLv3, Paul Kocialkowski) via
 * t113_self_parse. Single-planar only; controls target the 5.4 pre-stable
 * uapi (h264-ctrls-5.4.h).
 *
 * Buffer model: CAPTURE buffer index == DPB slot (see h264_dpb.h), owned by
 * userspace between decodes — a slot is only QBUFed for the frame decoding
 * into it. OUTPUT (bitstream) buffers are a small round-robin pool, each with
 * a persistent media request fd. Decode is synchronous (one frame in flight).
 */

#ifndef _VDEC_V4L2_H_
#define _VDEC_V4L2_H_

#include <stdbool.h>
#include <stdint.h>

#include "vdec.h"

#define VDEC_MAX_CAP_BUFS	32
#define VDEC_MAX_OUT_BUFS	4

struct vdec_cap_buf {
	void *map;
	unsigned int length;
	int dmabuf_fd;
};

struct vdec_out_buf {
	void *map;
	unsigned int length;
	int request_fd;
};

struct vdec_ctx {
	int video_fd;
	int media_fd;

	/* CAPTURE format as reported by the driver (SUNXI_TILED_NV12) */
	unsigned int cap_width;		/* == coded width (32-aligned) */
	unsigned int cap_height;	/* == coded height (32-aligned) */
	unsigned int cap_bytesperline;
	unsigned int cap_sizeimage;
	unsigned int cap_uv_offset;	/* luma plane size */
	unsigned int slow_threshold_us;	/* 0 = legacy 50ms diagnostic threshold */

	unsigned int cap_count;
	struct vdec_cap_buf cap[VDEC_MAX_CAP_BUFS];

	unsigned int out_count;
	unsigned int out_next;
	struct vdec_out_buf out[VDEC_MAX_OUT_BUFS];
};

/*
 * Scan /dev/video* + /dev/media* for the cedrus decoder. Fills the two path
 * buffers. Returns 0 on success.
 */
int vdec_find_device(char *video_path, unsigned int video_path_len,
		     char *media_path, unsigned int media_path_len);

/*
 * Open the decoder and set everything up through STREAMON: formats
 * (H264_SLICE -> SUNXI_TILED_NV12), buffer allocation, capture dmabuf export,
 * per-output-slot request fds, explicit DECODE_MODE/START_CODE.
 * `out_size` is the OUTPUT sizeimage (max NAL per frame).
 */
int vdec_open(struct vdec_ctx *v, const char *video_path,
	      const char *media_path, unsigned int width, unsigned int height,
	      unsigned int cap_count, unsigned int out_count,
	      unsigned int out_size);

void vdec_close(struct vdec_ctx *v);

/*
 * Decode one frame (single slice, raw NAL without start code) into capture
 * slot `cap_slot`, tagging it with `ts` (the DPB reference timestamp).
 * Synchronous; returns 0 when the frame is decoded.
 */
int vdec_decode(struct vdec_ctx *v, int cap_slot, uint64_t ts,
		const uint8_t *nal, unsigned int nal_size,
		struct vdec_h264_ctrls *ctrls);

#endif /* _VDEC_V4L2_H_ */
