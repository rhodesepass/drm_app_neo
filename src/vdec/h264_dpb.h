/*
 * H.264 decoded picture buffer (frame-only): reference management
 * (sliding window + MMCO), ref-list construction (P/B + modification),
 * V4L2 control DPB/ref-list filling, and display reordering.
 *
 * Each picture occupies a V4L2 CAPTURE buffer slot == its pool index. A slot
 * stays reserved while the picture is still a reference or not yet displayed.
 * References are keyed to the OUTPUT buffer timestamp (see Cedrus convention).
 */

#ifndef _VDEC_H264_DPB_H_
#define _VDEC_H264_DPB_H_

#include <stdbool.h>
#include <stdint.h>

#include "vdec.h"
#include "h264_parser.h"

#define H264_DPB_MAX_SLOTS	32

struct h264_dpb_pic {
	bool used;		/* occupies a v4l2 slot */
	int slot;		/* == index in pics[]; v4l2 buffer index */
	uint64_t ts;		/* OUTPUT timestamp used at decode time */

	int frame_num;
	int frame_num_wrap;
	int pic_num;		/* recomputed per current frame */
	int long_term_frame_idx;

	int32_t top_poc;
	int32_t bottom_poc;
	int32_t poc;		/* frame poc */

	bool is_ref;
	bool long_term;
	bool in_dpb;		/* still a reference */
	bool needed_for_output;	/* decoded but not yet displayed */
	bool on_screen;		/* handed to the display path (queued/scanout) */
};

struct h264_dpb {
	struct h264_dpb_pic pics[H264_DPB_MAX_SLOTS];
	int capacity;		/* number of v4l2 buffers */
	int max_num_ref_frames;
	int max_frame_num;
	int reorder_depth;
	int max_long_term_frame_idx;

	uint64_t ts_counter;
	int cur;		/* pool index of in-progress frame, -1 */
};

void h264_dpb_init(struct h264_dpb *dpb, int capacity, int max_num_ref_frames,
		   int max_frame_num, int reorder_depth);

/*
 * Begin a frame: build DPB/ref-list controls from existing references,
 * allocate a slot and a timestamp for the current picture. Returns the slot
 * index (>=0) and sets *ts_out, or -1 on failure (no free slot).
 */
int h264_dpb_begin_frame(struct h264_dpb *dpb, const struct h264_slice_hdr *hdr,
			 const struct h264_poc *poc, uint64_t *ts_out,
			 struct vdec_h264_ctrls *ctrl);

/* End a frame after successful decode: add to DPB (if reference) + marking. */
void h264_dpb_end_frame(struct h264_dpb *dpb, const struct h264_slice_hdr *hdr);

/* Abort the in-progress frame (decode failed): release its slot. */
void h264_dpb_abort_frame(struct h264_dpb *dpb);

/*
 * Next picture to display in output (POC) order. If flush, drain regardless of
 * reorder depth. Returns slot index (>=0) or -1 if nothing ready.
 */
int h264_dpb_next_output(struct h264_dpb *dpb, bool flush);

/* Mark a displayed slot; frees it if no longer referenced. */
void h264_dpb_mark_displayed(struct h264_dpb *dpb, int slot);

/*
 * Screen-out tracking: a slot handed to the display path must not be reused
 * until the frame is off screen, even if it is no longer a reference.
 */
void h264_dpb_set_on_screen(struct h264_dpb *dpb, int slot, bool on);

#endif /* _VDEC_H264_DPB_H_ */
