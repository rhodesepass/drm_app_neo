/*
 * Minimal H.264 bitstream parser: SPS/PPS + slice header, POC computation,
 * and conversion to V4L2 stateless controls.
 *
 * Field semantics follow ITU-T H.264 and the gst-plugins-bad parser; the
 * header_bit_size formula matches gstv4l2codech264dec.c.
 */

#ifndef _VDEC_H264_PARSER_H_
#define _VDEC_H264_PARSER_H_

#include <stdbool.h>
#include <stdint.h>

#include "nalu.h"
#include "vdec.h"

/* NAL unit types (subset) */
#define H264_NAL_SLICE		1
#define H264_NAL_DPA		2
#define H264_NAL_DPB		3
#define H264_NAL_DPC		4
#define H264_NAL_IDR		5
#define H264_NAL_SEI		6
#define H264_NAL_SPS		7
#define H264_NAL_PPS		8
#define H264_NAL_AU_DELIM	9

/* slice_type % 5 */
#define H264_SLICE_P	0
#define H264_SLICE_B	1
#define H264_SLICE_I	2
#define H264_SLICE_SP	3
#define H264_SLICE_SI	4

struct h264_sps {
	bool valid;
	uint8_t profile_idc;
	uint8_t constraint_set_flags;
	uint8_t level_idc;
	uint8_t seq_parameter_set_id;

	uint8_t chroma_format_idc;
	bool separate_colour_plane_flag;
	uint8_t bit_depth_luma_minus8;
	uint8_t bit_depth_chroma_minus8;
	bool qpprime_y_zero_transform_bypass_flag;

	bool seq_scaling_matrix_present_flag;
	uint8_t scaling_list_4x4[6][16];
	uint8_t scaling_list_8x8[6][64];

	uint8_t log2_max_frame_num_minus4;
	uint8_t pic_order_cnt_type;
	uint8_t log2_max_pic_order_cnt_lsb_minus4;
	bool delta_pic_order_always_zero_flag;
	int32_t offset_for_non_ref_pic;
	int32_t offset_for_top_to_bottom_field;
	uint8_t num_ref_frames_in_pic_order_cnt_cycle;
	int32_t offset_for_ref_frame[255];

	/* VUI bitstream_restriction：编码器承诺的 DPB 联合上限(refs 与重排
	 * 共享)。有它就不必按 max_ref+reorder 的不相交最差账开 capture */
	bool vui_reorder_valid;
	uint8_t vui_max_num_reorder_frames;
	uint8_t vui_max_dec_frame_buffering;

	uint8_t max_num_ref_frames;
	bool gaps_in_frame_num_value_allowed_flag;
	uint16_t pic_width_in_mbs_minus1;
	uint16_t pic_height_in_map_units_minus1;
	bool frame_mbs_only_flag;
	bool mb_adaptive_frame_field_flag;
	bool direct_8x8_inference_flag;

	bool frame_cropping_flag;
	uint32_t frame_crop_left_offset;
	uint32_t frame_crop_right_offset;
	uint32_t frame_crop_top_offset;
	uint32_t frame_crop_bottom_offset;
};

struct h264_pps {
	bool valid;
	uint8_t pic_parameter_set_id;
	uint8_t seq_parameter_set_id;
	bool entropy_coding_mode_flag;
	bool bottom_field_pic_order_in_frame_present_flag;
	uint8_t num_slice_groups_minus1;
	uint8_t num_ref_idx_l0_default_active_minus1;
	uint8_t num_ref_idx_l1_default_active_minus1;
	bool weighted_pred_flag;
	uint8_t weighted_bipred_idc;
	int8_t pic_init_qp_minus26;
	int8_t pic_init_qs_minus26;
	int8_t chroma_qp_index_offset;
	bool deblocking_filter_control_present_flag;
	bool constrained_intra_pred_flag;
	bool redundant_pic_cnt_present_flag;
	bool transform_8x8_mode_flag;
	bool pic_scaling_matrix_present_flag;
	int8_t second_chroma_qp_index_offset;
	uint8_t scaling_list_4x4[6][16];
	uint8_t scaling_list_8x8[6][64];
};

struct h264_rplm {
	uint8_t idc;		/* modification_of_pic_nums_idc */
	uint32_t val;		/* abs_diff_pic_num_minus1 or long_term_pic_num */
};

struct h264_mmco {
	uint8_t op;		/* memory_management_control_operation */
	uint32_t arg1;
	uint32_t arg2;
};

/* Parsed slice header (the fields we need). */
struct h264_slice_hdr {
	uint8_t nal_ref_idc;
	uint8_t nal_unit_type;
	bool idr;

	uint32_t first_mb_in_slice;
	uint8_t slice_type;		/* raw (0..9) */
	uint8_t pic_parameter_set_id;
	uint8_t colour_plane_id;
	uint16_t frame_num;
	bool field_pic_flag;
	bool bottom_field_flag;
	uint16_t idr_pic_id;
	uint16_t pic_order_cnt_lsb;
	int32_t delta_pic_order_cnt_bottom;
	int32_t delta_pic_order_cnt[2];
	uint8_t redundant_pic_cnt;
	bool direct_spatial_mv_pred_flag;
	bool num_ref_idx_active_override_flag;
	uint8_t num_ref_idx_l0_active_minus1;
	uint8_t num_ref_idx_l1_active_minus1;
	uint8_t cabac_init_idc;
	int8_t slice_qp_delta;
	bool sp_for_switch_flag;
	int8_t slice_qs_delta;
	uint8_t disable_deblocking_filter_idc;
	int8_t slice_alpha_c0_offset_div2;
	int8_t slice_beta_offset_div2;

	/* ref_pic_list_modification */
	bool ref_pic_list_modification_flag_l0;
	bool ref_pic_list_modification_flag_l1;
	struct h264_rplm rplm_l0[32];
	struct h264_rplm rplm_l1[32];
	uint8_t n_rplm_l0;
	uint8_t n_rplm_l1;

	/* dec_ref_pic_marking */
	bool no_output_of_prior_pics_flag;
	bool long_term_reference_flag;
	bool adaptive_ref_pic_marking_mode_flag;
	struct h264_mmco mmco[32];
	uint8_t n_mmco;

	/* prediction weight table */
	uint16_t luma_log2_weight_denom;
	uint16_t chroma_log2_weight_denom;
	struct v4l2_h264_weight_factors weights[2];
	bool has_pred_weights;

	/* bit accounting (raw-domain, see bitreader.h) */
	uint32_t header_size;			/* bits */
	uint32_t n_emulation_prevention_bytes;
	uint32_t pic_order_cnt_bit_size;	/* bits */
	uint32_t dec_ref_pic_marking_bit_size;	/* bits */
};

/* Per-frame POC result. */
struct h264_poc {
	int32_t top_field_order_cnt;
	int32_t bottom_field_order_cnt;
	int32_t poc;		/* min of top/bottom for frames */
};

struct h264_parser {
	struct h264_sps sps[32];
	struct h264_pps pps[256];

	/* POC state (carried across frames) */
	int32_t prev_poc_msb;
	int32_t prev_poc_lsb;
	int32_t prev_frame_num;
	int32_t prev_frame_num_offset;
	bool have_prev;
};

void h264_parser_init(struct h264_parser *p);

/* Parse the avcC config record, registering its SPS/PPS. */
int h264_parser_parse_avcc(struct h264_parser *p, const uint8_t *avcc,
			   unsigned int size);

/* Parse a parameter-set NAL (SPS or PPS). Returns 0 on success, <0 ignore. */
int h264_parser_parse_param_nal(struct h264_parser *p, const struct nalu *n);

/* Parse a VCL slice NAL into `hdr`. Returns 0 on success. */
int h264_parser_parse_slice(struct h264_parser *p, const struct nalu *n,
			    struct h264_slice_hdr *hdr);

/* Compute POC for the (first slice of the) frame and update POC state. */
void h264_parser_compute_poc(struct h264_parser *p,
			     const struct h264_slice_hdr *hdr,
			     struct h264_poc *out);

/* Fill the static (non-DPB) parts of the V4L2 controls from sps/pps/slice. */
int h264_parser_fill_controls(struct h264_parser *p,
			      const struct h264_slice_hdr *hdr,
			      const struct h264_poc *poc,
			      struct vdec_h264_ctrls *ctrl);

/* Accessors used by the decoder/DPB layer. */
const struct h264_sps *h264_parser_get_sps(const struct h264_parser *p, int id);
const struct h264_pps *h264_parser_get_pps(const struct h264_parser *p, int id);

#endif /* _VDEC_H264_PARSER_H_ */
