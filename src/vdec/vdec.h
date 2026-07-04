/*
 * Shared definitions for the self-contained H.264/MP4 decode stack (src/vdec).
 *
 * Ported from t113_self_parse; V4L2 structs target the 5.4 pre-stable
 * stateless uapi (see h264-ctrls-5.4.h). Parser/DPB compile host-side
 * without any system media header.
 */

#ifndef _VDEC_H_
#define _VDEC_H_

#include "h264-ctrls-5.4.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* One frame's worth of controls, submitted per request. */
struct vdec_h264_ctrls {
	struct v4l2_ctrl_h264_decode_params decode_params;
	struct v4l2_ctrl_h264_pps pps;
	struct v4l2_ctrl_h264_scaling_matrix scaling_matrix;
	struct v4l2_ctrl_h264_slice_params slice_params;
	struct v4l2_ctrl_h264_sps sps;
};

#endif /* _VDEC_H_ */
