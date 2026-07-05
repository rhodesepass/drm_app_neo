/*
 * H.264 SPS/PPS/slice parser + POC + V4L2 control filling. See h264_parser.h.
 *
 * Slice-header bit accounting (header_bit_size, pic_order_cnt_bit_size,
 * dec_ref_pic_marking_bit_size) follows gst-plugins-bad's gsth264parser.c /
 * gstv4l2codech264dec.c exactly.
 */

#include "h264_parser.h"

#include <string.h>

#include "bitreader.h"

void h264_parser_init(struct h264_parser *p)
{
	memset(p, 0, sizeof(*p));
}

const struct h264_sps *h264_parser_get_sps(const struct h264_parser *p, int id)
{
	if (id < 0 || id >= 32 || !p->sps[id].valid)
		return NULL;
	return &p->sps[id];
}

const struct h264_pps *h264_parser_get_pps(const struct h264_parser *p, int id)
{
	if (id < 0 || id >= 256 || !p->pps[id].valid)
		return NULL;
	return &p->pps[id];
}

/*
 * Scaling lists (H.264 7.3.2.1.1.1 / 8.5.9). The bitstream carries values in
 * up-right zig-zag scan order; V4L2 (v4l2_ctrl_h264_scaling_matrix) expects
 * raster order, mirroring gst's gst_h264_quant_matrix_*_get_raster_from_zigzag.
 * Default matrices below are in zig-zag order, as listed in spec Table 7-3.
 */
static const uint8_t default_4x4_intra[16] = {
	6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32, 32, 37, 37, 42,
};
static const uint8_t default_4x4_inter[16] = {
	10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27, 27, 30, 30, 34,
};
static const uint8_t default_8x8_intra[64] = {
	 6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
	23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
	27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
	31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42,
};
static const uint8_t default_8x8_inter[64] = {
	 9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
	21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
	24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
	27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35,
};
static const uint8_t zigzag_4x4[16] = {
	0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};
static const uint8_t zigzag_8x8[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

static void scan_to_raster(const uint8_t *scan, uint8_t *raster,
			   const uint8_t *zz, unsigned int size)
{
	unsigned int k;

	for (k = 0; k < size; k++)
		raster[zz[k]] = scan[k];
}

/* Parse one scaling_list() into scan-order `out`. Sets *use_default when the
 * default-matrix escape (nextScale==0 at j==0) fires. */
static void parse_scaling_list(struct bitreader *br, uint8_t *out,
			       unsigned int size, bool *use_default)
{
	int last_scale = 8, next_scale = 8, cur;
	unsigned int j;

	*use_default = false;

	for (j = 0; j < size; j++) {
		if (next_scale != 0) {
			int delta = br_se_v(br);
			next_scale = (last_scale + delta + 256) % 256;
			if (j == 0 && next_scale == 0)
				*use_default = true;
		}
		cur = (next_scale == 0) ? last_scale : next_scale;
		out[j] = (uint8_t)cur;
		last_scale = cur;
	}
}

static void set_default_4x4(uint8_t dst[16], unsigned int i)
{
	scan_to_raster(i < 3 ? default_4x4_intra : default_4x4_inter,
		       dst, zigzag_4x4, 16);
}

static void set_default_8x8(uint8_t dst[64], unsigned int idx)
{
	scan_to_raster((idx & 1) ? default_8x8_inter : default_8x8_intra,
		       dst, zigzag_8x8, 64);
}

/*
 * Derive all scaling lists for an SPS (fall-back rule set A) or PPS (rule set
 * B). For PPS, `sps` supplies rule-B references (NULL/seq-absent -> defaults).
 * Stored raster order into out4x4/out8x8.
 */
static void parse_scaling_lists(struct bitreader *br, bool transform_8x8,
				uint8_t chroma_format_idc,
				uint8_t out4x4[6][16], uint8_t out8x8[6][64],
				const struct h264_sps *sps)
{
	unsigned int n8x8 = (chroma_format_idc == 3) ? 6 : 2;
	bool seq_present = sps && sps->seq_scaling_matrix_present_flag;
	uint8_t scan[64];
	bool present, use_default;
	unsigned int i;

	for (i = 0; i < 6; i++) {
		present = br_u1(br);	/* scaling_list_present_flag[i] */
		if (present) {
			parse_scaling_list(br, scan, 16, &use_default);
			if (use_default)
				set_default_4x4(out4x4[i], i);
			else
				scan_to_raster(scan, out4x4[i], zigzag_4x4, 16);
		} else if (i == 0) {
			if (seq_present)
				memcpy(out4x4[0], sps->scaling_list_4x4[0], 16);
			else
				set_default_4x4(out4x4[0], 0);
		} else if (i == 3) {
			if (seq_present)
				memcpy(out4x4[3], sps->scaling_list_4x4[3], 16);
			else
				set_default_4x4(out4x4[3], 3);
		} else {
			memcpy(out4x4[i], out4x4[i - 1], 16);
		}
	}

	if (!transform_8x8)
		return;

	for (i = 0; i < n8x8; i++) {
		present = br_u1(br);
		if (present) {
			parse_scaling_list(br, scan, 64, &use_default);
			if (use_default)
				set_default_8x8(out8x8[i], i);
			else
				scan_to_raster(scan, out8x8[i], zigzag_8x8, 64);
		} else if (i == 0) {
			if (seq_present)
				memcpy(out8x8[0], sps->scaling_list_8x8[0], 64);
			else
				set_default_8x8(out8x8[0], 0);
		} else if (i == 1) {
			if (seq_present)
				memcpy(out8x8[1], sps->scaling_list_8x8[1], 64);
			else
				set_default_8x8(out8x8[1], 1);
		} else {
			memcpy(out8x8[i], out8x8[i - 2], 64);
		}
	}
}

static void skip_hrd_parameters(struct bitreader *br)
{
	uint32_t cpb_cnt_minus1 = br_ue_v(br);
	unsigned int i;

	br_u(br, 8);			/* bit_rate_scale + cpb_size_scale */
	for (i = 0; i <= cpb_cnt_minus1 && i < 32; i++) {
		br_ue_v(br);		/* bit_rate_value_minus1 */
		br_ue_v(br);		/* cpb_size_value_minus1 */
		br_u1(br);		/* cbr_flag */
	}
	br_u(br, 20);			/* 4 个 5bit 长度字段 */
}

/*
 * VUI 只取 bitstream_restriction 的 DPB 上限(决定 capture buffer 数)，
 * 其余全部跳读。bitreader 越界读返回 0，最后做合理性校验兜底。
 */
static void parse_vui(struct bitreader *br, struct h264_sps *s)
{
	bool nal_hrd, vcl_hrd;
	uint32_t reorder, dec_buf;

	if (br_u1(br)) {		/* aspect_ratio_info_present */
		if (br_u(br, 8) == 255)	/* Extended_SAR */
			br_u(br, 32);
	}
	if (br_u1(br))			/* overscan_info_present */
		br_u1(br);
	if (br_u1(br)) {		/* video_signal_type_present */
		br_u(br, 4);		/* video_format + full_range */
		if (br_u1(br))		/* colour_description_present */
			br_u(br, 24);
	}
	if (br_u1(br)) {		/* chroma_loc_info_present */
		br_ue_v(br);
		br_ue_v(br);
	}
	if (br_u1(br)) {		/* timing_info_present */
		br_u(br, 32);		/* num_units_in_tick */
		br_u(br, 32);		/* time_scale */
		br_u1(br);		/* fixed_frame_rate */
	}
	nal_hrd = br_u1(br);
	if (nal_hrd)
		skip_hrd_parameters(br);
	vcl_hrd = br_u1(br);
	if (vcl_hrd)
		skip_hrd_parameters(br);
	if (nal_hrd || vcl_hrd)
		br_u1(br);		/* low_delay_hrd */
	br_u1(br);			/* pic_struct_present */
	if (!br_u1(br))			/* bitstream_restriction */
		return;

	br_u1(br);			/* mv_over_pic_boundaries */
	br_ue_v(br);			/* max_bytes_per_pic_denom */
	br_ue_v(br);			/* max_bits_per_mb_denom */
	br_ue_v(br);			/* log2_max_mv_length_horizontal */
	br_ue_v(br);			/* log2_max_mv_length_vertical */
	reorder = br_ue_v(br);
	dec_buf = br_ue_v(br);

	if (dec_buf >= 1 && dec_buf <= 16 && reorder <= dec_buf &&
	    dec_buf >= s->max_num_ref_frames) {
		s->vui_max_num_reorder_frames = reorder;
		s->vui_max_dec_frame_buffering = dec_buf;
		s->vui_reorder_valid = true;
	}
}

int h264_parser_parse_param_nal(struct h264_parser *p, const struct nalu *n)
{
	unsigned int type = nalu_h264_type(n);
	struct bitreader br;

	if (n->size < 1)
		return -1;

	/* parse RBSP after the 1-byte NAL header */
	br_init(&br, n->data + 1, n->size - 1);

	if (type == H264_NAL_SPS) {
		struct h264_sps s;
		uint32_t id;
		unsigned int i;

		memset(&s, 0, sizeof(s));
		s.profile_idc = br_u(&br, 8);
		s.constraint_set_flags = br_u(&br, 8); /* constraint flags + reserved */
		s.level_idc = br_u(&br, 8);
		id = br_ue_v(&br);
		if (id >= 32)
			return -1;
		s.seq_parameter_set_id = id;

		s.chroma_format_idc = 1;
		if (s.profile_idc == 100 || s.profile_idc == 110 ||
		    s.profile_idc == 122 || s.profile_idc == 244 ||
		    s.profile_idc == 44 || s.profile_idc == 83 ||
		    s.profile_idc == 86 || s.profile_idc == 118 ||
		    s.profile_idc == 128 || s.profile_idc == 138 ||
		    s.profile_idc == 139 || s.profile_idc == 134 ||
		    s.profile_idc == 135) {
			s.chroma_format_idc = br_ue_v(&br);
			if (s.chroma_format_idc == 3)
				s.separate_colour_plane_flag = br_u1(&br);
			s.bit_depth_luma_minus8 = br_ue_v(&br);
			s.bit_depth_chroma_minus8 = br_ue_v(&br);
			s.qpprime_y_zero_transform_bypass_flag = br_u1(&br);
			s.seq_scaling_matrix_present_flag = br_u1(&br);
			if (s.seq_scaling_matrix_present_flag)
				parse_scaling_lists(&br, true, s.chroma_format_idc,
						    s.scaling_list_4x4,
						    s.scaling_list_8x8, NULL);
		}

		s.log2_max_frame_num_minus4 = br_ue_v(&br);
		s.pic_order_cnt_type = br_ue_v(&br);
		if (s.pic_order_cnt_type == 0) {
			s.log2_max_pic_order_cnt_lsb_minus4 = br_ue_v(&br);
		} else if (s.pic_order_cnt_type == 1) {
			s.delta_pic_order_always_zero_flag = br_u1(&br);
			s.offset_for_non_ref_pic = br_se_v(&br);
			s.offset_for_top_to_bottom_field = br_se_v(&br);
			s.num_ref_frames_in_pic_order_cnt_cycle = br_ue_v(&br);
			for (i = 0; i < s.num_ref_frames_in_pic_order_cnt_cycle &&
				    i < 255; i++)
				s.offset_for_ref_frame[i] = br_se_v(&br);
		}

		s.max_num_ref_frames = br_ue_v(&br);
		s.gaps_in_frame_num_value_allowed_flag = br_u1(&br);
		s.pic_width_in_mbs_minus1 = br_ue_v(&br);
		s.pic_height_in_map_units_minus1 = br_ue_v(&br);
		s.frame_mbs_only_flag = br_u1(&br);
		if (!s.frame_mbs_only_flag)
			s.mb_adaptive_frame_field_flag = br_u1(&br);
		s.direct_8x8_inference_flag = br_u1(&br);
		s.frame_cropping_flag = br_u1(&br);
		if (s.frame_cropping_flag) {
			s.frame_crop_left_offset = br_ue_v(&br);
			s.frame_crop_right_offset = br_ue_v(&br);
			s.frame_crop_top_offset = br_ue_v(&br);
			s.frame_crop_bottom_offset = br_ue_v(&br);
		}
		if (br_u1(&br))		/* vui_parameters_present */
			parse_vui(&br, &s);

		s.valid = true;
		p->sps[s.seq_parameter_set_id] = s;
		return 0;
	}

	if (type == H264_NAL_PPS) {
		struct h264_pps pp;
		const struct h264_sps *sps;
		uint32_t id, sps_id;

		memset(&pp, 0, sizeof(pp));
		id = br_ue_v(&br);
		if (id >= 256)
			return -1;
		pp.pic_parameter_set_id = id;
		sps_id = br_ue_v(&br);
		if (sps_id >= 32)
			return -1;
		pp.seq_parameter_set_id = sps_id;
		sps = h264_parser_get_sps(p, sps_id);

		pp.entropy_coding_mode_flag = br_u1(&br);
		pp.bottom_field_pic_order_in_frame_present_flag = br_u1(&br);
		pp.num_slice_groups_minus1 = br_ue_v(&br);
		if (pp.num_slice_groups_minus1 > 0) {
			/* slice group map syntax not needed for cedrus targets;
			 * such streams are uncommon. Bail to flat handling. */
		}
		pp.num_ref_idx_l0_default_active_minus1 = br_ue_v(&br);
		pp.num_ref_idx_l1_default_active_minus1 = br_ue_v(&br);
		pp.weighted_pred_flag = br_u1(&br);
		pp.weighted_bipred_idc = br_u(&br, 2);
		pp.pic_init_qp_minus26 = br_se_v(&br);
		pp.pic_init_qs_minus26 = br_se_v(&br);
		pp.chroma_qp_index_offset = br_se_v(&br);
		pp.deblocking_filter_control_present_flag = br_u1(&br);
		pp.constrained_intra_pred_flag = br_u1(&br);
		pp.redundant_pic_cnt_present_flag = br_u1(&br);
		pp.second_chroma_qp_index_offset = pp.chroma_qp_index_offset;

		if (br_more_rbsp_data(&br)) {
			pp.transform_8x8_mode_flag = br_u1(&br);
			pp.pic_scaling_matrix_present_flag = br_u1(&br);
			if (pp.pic_scaling_matrix_present_flag) {
				uint8_t cfi = sps ? sps->chroma_format_idc : 1;
				parse_scaling_lists(&br, pp.transform_8x8_mode_flag,
						    cfi, pp.scaling_list_4x4,
						    pp.scaling_list_8x8, sps);
			}
			pp.second_chroma_qp_index_offset = br_se_v(&br);
		}

		pp.valid = true;
		p->pps[pp.pic_parameter_set_id] = pp;
		return 0;
	}

	return -1;
}

int h264_parser_parse_avcc(struct h264_parser *p, const uint8_t *avcc,
			   unsigned int size)
{
	unsigned int pos = 5; /* skip version, profile, compat, level, lenSize */
	unsigned int i, count;

	if (size < 7)
		return -1;

	count = avcc[pos++] & 0x1f;	/* numOfSequenceParameterSets */
	for (i = 0; i < count; i++) {
		unsigned int len;
		struct nalu n;
		if (pos + 2 > size)
			return -1;
		len = (avcc[pos] << 8) | avcc[pos + 1];
		pos += 2;
		if (pos + len > size)
			return -1;
		n.data = avcc + pos;
		n.size = len;
		h264_parser_parse_param_nal(p, &n);
		pos += len;
	}

	if (pos >= size)
		return -1;
	count = avcc[pos++];		/* numOfPictureParameterSets */
	for (i = 0; i < count; i++) {
		unsigned int len;
		struct nalu n;
		if (pos + 2 > size)
			return -1;
		len = (avcc[pos] << 8) | avcc[pos + 1];
		pos += 2;
		if (pos + len > size)
			return -1;
		n.data = avcc + pos;
		n.size = len;
		h264_parser_parse_param_nal(p, &n);
		pos += len;
	}

	return 0;
}

static void parse_one_rplm(struct bitreader *br, bool *flag,
			   struct h264_rplm *list, uint8_t *count)
{
	*flag = br_u1(br);
	if (!*flag)
		return;
	for (;;) {
		uint32_t idc = br_ue_v(br);
		if (idc == 3)
			break;
		if (*count < 32) {
			list[*count].idc = idc;
			list[*count].val = br_ue_v(br); /* abs_diff or lt_pic_num */
			(*count)++;
		} else {
			br_ue_v(br);
		}
	}
}

static void parse_ref_pic_list_modification(struct bitreader *br,
					    struct h264_slice_hdr *h)
{
	uint8_t st = h->slice_type % 5;

	if (st != H264_SLICE_I && st != H264_SLICE_SI)
		parse_one_rplm(br, &h->ref_pic_list_modification_flag_l0,
			       h->rplm_l0, &h->n_rplm_l0);
	if (st == H264_SLICE_B)
		parse_one_rplm(br, &h->ref_pic_list_modification_flag_l1,
			       h->rplm_l1, &h->n_rplm_l1);
}

static void parse_pred_weight_table(struct bitreader *br,
				    struct h264_slice_hdr *h,
				    uint8_t chroma_array_type)
{
	uint8_t st = h->slice_type % 5;
	int num_l0 = h->num_ref_idx_l0_active_minus1 + 1;
	int num_l1 = h->num_ref_idx_l1_active_minus1 + 1;
	int list, i, j;

	h->luma_log2_weight_denom = br_ue_v(br);
	if (chroma_array_type != 0)
		h->chroma_log2_weight_denom = br_ue_v(br);
	h->has_pred_weights = true;

	for (list = 0; list < 2; list++) {
		int num = (list == 0) ? num_l0 : num_l1;
		struct v4l2_h264_weight_factors *wf = &h->weights[list];

		if (list == 1 && st != H264_SLICE_B)
			break;

		for (i = 0; i < num && i < 32; i++) {
			/* defaults */
			wf->luma_weight[i] = 1 << h->luma_log2_weight_denom;
			wf->luma_offset[i] = 0;
			for (j = 0; j < 2; j++) {
				wf->chroma_weight[i][j] =
					1 << h->chroma_log2_weight_denom;
				wf->chroma_offset[i][j] = 0;
			}

			if (br_u1(br)) {	/* luma_weight_lX_flag */
				wf->luma_weight[i] = br_se_v(br);
				wf->luma_offset[i] = br_se_v(br);
			}
			if (chroma_array_type != 0) {
				if (br_u1(br)) {	/* chroma_weight_lX_flag */
					for (j = 0; j < 2; j++) {
						wf->chroma_weight[i][j] = br_se_v(br);
						wf->chroma_offset[i][j] = br_se_v(br);
					}
				}
			}
		}
	}
}

static void parse_dec_ref_pic_marking(struct bitreader *br,
				      struct h264_slice_hdr *h)
{
	unsigned int start_pos = br_pos(br);
	unsigned int start_epb = br_epb_count(br);

	if (h->idr) {
		h->no_output_of_prior_pics_flag = br_u1(br);
		h->long_term_reference_flag = br_u1(br);
	} else {
		h->adaptive_ref_pic_marking_mode_flag = br_u1(br);
		if (h->adaptive_ref_pic_marking_mode_flag) {
			uint32_t op;
			do {
				uint32_t a1 = 0, a2 = 0;
				op = br_ue_v(br);
				if (op == 1 || op == 3)
					a1 = br_ue_v(br); /* difference_of_pic_nums_minus1 */
				if (op == 2)
					a1 = br_ue_v(br); /* long_term_pic_num */
				if (op == 3 || op == 6)
					a2 = br_ue_v(br); /* long_term_frame_idx */
				if (op == 4)
					a1 = br_ue_v(br); /* max_long_term_frame_idx_plus1 */
				if (op != 0 && h->n_mmco < 32) {
					h->mmco[h->n_mmco].op = op;
					h->mmco[h->n_mmco].arg1 = a1;
					h->mmco[h->n_mmco].arg2 = a2;
					h->n_mmco++;
				}
			} while (op != 0);
		}
	}

	h->dec_ref_pic_marking_bit_size =
		(br_pos(br) - start_pos) -
		8 * (br_epb_count(br) - start_epb);
}

int h264_parser_parse_slice(struct h264_parser *p, const struct nalu *n,
			    struct h264_slice_hdr *h)
{
	struct bitreader br;
	const struct h264_sps *sps;
	const struct h264_pps *pps;
	uint8_t chroma_array_type;
	unsigned int start_pos, start_epb;

	if (n->size < 1)
		return -1;

	memset(h, 0, sizeof(*h));
	h->nal_ref_idc = (n->data[0] >> 5) & 0x3;
	h->nal_unit_type = n->data[0] & 0x1f;
	h->idr = (h->nal_unit_type == H264_NAL_IDR);

	br_init(&br, n->data + 1, n->size - 1);

	h->first_mb_in_slice = br_ue_v(&br);
	h->slice_type = br_ue_v(&br);
	h->pic_parameter_set_id = br_ue_v(&br);

	pps = h264_parser_get_pps(p, h->pic_parameter_set_id);
	if (!pps)
		return -1;
	sps = h264_parser_get_sps(p, pps->seq_parameter_set_id);
	if (!sps)
		return -1;
	chroma_array_type = sps->separate_colour_plane_flag ?
		0 : sps->chroma_format_idc;

	if (sps->separate_colour_plane_flag)
		h->colour_plane_id = br_u(&br, 2);

	h->frame_num = br_u(&br, sps->log2_max_frame_num_minus4 + 4);

	if (!sps->frame_mbs_only_flag) {
		h->field_pic_flag = br_u1(&br);
		if (h->field_pic_flag)
			h->bottom_field_flag = br_u1(&br);
	}

	if (h->idr)
		h->idr_pic_id = br_ue_v(&br);

	start_pos = br_pos(&br);
	start_epb = br_epb_count(&br);

	if (sps->pic_order_cnt_type == 0) {
		h->pic_order_cnt_lsb =
			br_u(&br, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
		if (pps->bottom_field_pic_order_in_frame_present_flag &&
		    !h->field_pic_flag)
			h->delta_pic_order_cnt_bottom = br_se_v(&br);
	}
	if (sps->pic_order_cnt_type == 1 &&
	    !sps->delta_pic_order_always_zero_flag) {
		h->delta_pic_order_cnt[0] = br_se_v(&br);
		if (pps->bottom_field_pic_order_in_frame_present_flag &&
		    !h->field_pic_flag)
			h->delta_pic_order_cnt[1] = br_se_v(&br);
	}

	h->pic_order_cnt_bit_size =
		(br_pos(&br) - start_pos) - 8 * (br_epb_count(&br) - start_epb);

	if (pps->redundant_pic_cnt_present_flag)
		h->redundant_pic_cnt = br_ue_v(&br);

	if ((h->slice_type % 5) == H264_SLICE_B)
		h->direct_spatial_mv_pred_flag = br_u1(&br);

	h->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
	h->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
	{
		uint8_t st = h->slice_type % 5;
		if (st == H264_SLICE_P || st == H264_SLICE_SP ||
		    st == H264_SLICE_B) {
			h->num_ref_idx_active_override_flag = br_u1(&br);
			if (h->num_ref_idx_active_override_flag) {
				h->num_ref_idx_l0_active_minus1 = br_ue_v(&br);
				if (st == H264_SLICE_B)
					h->num_ref_idx_l1_active_minus1 =
						br_ue_v(&br);
			}
		}
	}

	parse_ref_pic_list_modification(&br, h);

	if ((pps->weighted_pred_flag &&
	     ((h->slice_type % 5) == H264_SLICE_P ||
	      (h->slice_type % 5) == H264_SLICE_SP)) ||
	    (pps->weighted_bipred_idc == 1 &&
	     (h->slice_type % 5) == H264_SLICE_B))
		parse_pred_weight_table(&br, h, chroma_array_type);

	if (h->nal_ref_idc != 0)
		parse_dec_ref_pic_marking(&br, h);

	if (pps->entropy_coding_mode_flag &&
	    (h->slice_type % 5) != H264_SLICE_I &&
	    (h->slice_type % 5) != H264_SLICE_SI)
		h->cabac_init_idc = br_ue_v(&br);

	h->slice_qp_delta = br_se_v(&br);

	if ((h->slice_type % 5) == H264_SLICE_SP ||
	    (h->slice_type % 5) == H264_SLICE_SI) {
		if ((h->slice_type % 5) == H264_SLICE_SP)
			h->sp_for_switch_flag = br_u1(&br);
		h->slice_qs_delta = br_se_v(&br);
	}

	if (pps->deblocking_filter_control_present_flag) {
		h->disable_deblocking_filter_idc = br_ue_v(&br);
		if (h->disable_deblocking_filter_idc != 1) {
			h->slice_alpha_c0_offset_div2 = br_se_v(&br);
			h->slice_beta_offset_div2 = br_se_v(&br);
		}
	}

	/* num_slice_groups_minus1 > 0 map handling skipped (uncommon). */

	h->header_size = br_pos(&br);
	h->n_emulation_prevention_bytes = br_epb_count(&br);

	(void)chroma_array_type;
	return 0;
}

void h264_parser_compute_poc(struct h264_parser *p,
			     const struct h264_slice_hdr *h,
			     struct h264_poc *out)
{
	const struct h264_pps *pps = h264_parser_get_pps(p, h->pic_parameter_set_id);
	const struct h264_sps *sps = pps ? h264_parser_get_sps(p, pps->seq_parameter_set_id) : NULL;
	int32_t top = 0, bottom = 0;

	memset(out, 0, sizeof(*out));
	if (!sps)
		return;

	if (sps->pic_order_cnt_type == 0) {
		int32_t max_poc_lsb =
			1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
		int32_t prev_msb, prev_lsb, poc_msb;

		if (h->idr) {
			prev_msb = 0;
			prev_lsb = 0;
		} else {
			prev_msb = p->prev_poc_msb;
			prev_lsb = p->prev_poc_lsb;
		}

		if (h->pic_order_cnt_lsb < prev_lsb &&
		    (prev_lsb - h->pic_order_cnt_lsb) >= max_poc_lsb / 2)
			poc_msb = prev_msb + max_poc_lsb;
		else if (h->pic_order_cnt_lsb > prev_lsb &&
			 (h->pic_order_cnt_lsb - prev_lsb) > max_poc_lsb / 2)
			poc_msb = prev_msb - max_poc_lsb;
		else
			poc_msb = prev_msb;

		top = poc_msb + h->pic_order_cnt_lsb;
		bottom = top;
		if (!h->field_pic_flag)
			bottom = top + h->delta_pic_order_cnt_bottom;

		if (h->nal_ref_idc != 0) {
			p->prev_poc_msb = poc_msb;
			p->prev_poc_lsb = h->pic_order_cnt_lsb;
		}
	} else if (sps->pic_order_cnt_type == 1) {
		int32_t max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
		int32_t frame_num_offset, abs_frame_num, expected_poc;
		int32_t cycle_cnt = 0, frame_in_cycle = 0;
		int32_t expected_delta = 0;
		int i;

		if (h->idr)
			frame_num_offset = 0;
		else if (p->prev_frame_num > h->frame_num)
			frame_num_offset = p->prev_frame_num_offset + max_frame_num;
		else
			frame_num_offset = p->prev_frame_num_offset;

		for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			expected_delta += sps->offset_for_ref_frame[i];

		if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
			abs_frame_num = frame_num_offset + h->frame_num;
		else
			abs_frame_num = 0;
		if (h->nal_ref_idc == 0 && abs_frame_num > 0)
			abs_frame_num--;

		if (abs_frame_num > 0) {
			cycle_cnt = (abs_frame_num - 1) /
				sps->num_ref_frames_in_pic_order_cnt_cycle;
			frame_in_cycle = (abs_frame_num - 1) %
				sps->num_ref_frames_in_pic_order_cnt_cycle;
			expected_poc = cycle_cnt * expected_delta;
			for (i = 0; i <= frame_in_cycle; i++)
				expected_poc += sps->offset_for_ref_frame[i];
		} else {
			expected_poc = 0;
		}
		if (h->nal_ref_idc == 0)
			expected_poc += sps->offset_for_non_ref_pic;

		top = expected_poc + h->delta_pic_order_cnt[0];
		bottom = top + sps->offset_for_top_to_bottom_field +
			 h->delta_pic_order_cnt[1];

		p->prev_frame_num_offset = frame_num_offset;
		p->prev_frame_num = h->frame_num;
	} else { /* type 2 */
		int32_t max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
		int32_t frame_num_offset, tmp_poc;

		if (h->idr)
			frame_num_offset = 0;
		else if (p->prev_frame_num > h->frame_num)
			frame_num_offset = p->prev_frame_num_offset + max_frame_num;
		else
			frame_num_offset = p->prev_frame_num_offset;

		if (h->idr)
			tmp_poc = 0;
		else if (h->nal_ref_idc == 0)
			tmp_poc = 2 * (frame_num_offset + h->frame_num) - 1;
		else
			tmp_poc = 2 * (frame_num_offset + h->frame_num);

		top = tmp_poc;
		bottom = tmp_poc;

		p->prev_frame_num_offset = frame_num_offset;
		p->prev_frame_num = h->frame_num;
	}

	out->top_field_order_cnt = top;
	out->bottom_field_order_cnt = bottom;
	out->poc = (top < bottom) ? top : bottom;
	if (h->field_pic_flag)
		out->poc = h->bottom_field_flag ? bottom : top;

	p->have_prev = true;
}

int h264_parser_fill_controls(struct h264_parser *p,
			      const struct h264_slice_hdr *h,
			      const struct h264_poc *poc,
			      struct vdec_h264_ctrls *ctrl)
{
	const struct h264_pps *pps = h264_parser_get_pps(p, h->pic_parameter_set_id);
	const struct h264_sps *sps = pps ? h264_parser_get_sps(p, pps->seq_parameter_set_id) : NULL;
	struct v4l2_ctrl_h264_sps *vsps = &ctrl->sps;
	struct v4l2_ctrl_h264_pps *vpps = &ctrl->pps;
	struct v4l2_ctrl_h264_slice_params *vsl = &ctrl->slice_params;
	struct v4l2_ctrl_h264_decode_params *vdp = &ctrl->decode_params;

	if (!sps || !pps)
		return -1;

	memset(ctrl, 0, sizeof(*ctrl));

	/* SPS */
	vsps->profile_idc = sps->profile_idc;
	vsps->constraint_set_flags = sps->constraint_set_flags;
	vsps->level_idc = sps->level_idc;
	vsps->seq_parameter_set_id = sps->seq_parameter_set_id;
	vsps->chroma_format_idc = sps->chroma_format_idc;
	vsps->bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
	vsps->bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
	vsps->log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
	vsps->pic_order_cnt_type = sps->pic_order_cnt_type;
	vsps->log2_max_pic_order_cnt_lsb_minus4 =
		sps->log2_max_pic_order_cnt_lsb_minus4;
	vsps->max_num_ref_frames = sps->max_num_ref_frames;
	vsps->num_ref_frames_in_pic_order_cnt_cycle =
		sps->num_ref_frames_in_pic_order_cnt_cycle;
	memcpy(vsps->offset_for_ref_frame, sps->offset_for_ref_frame,
	       sizeof(vsps->offset_for_ref_frame));
	vsps->offset_for_non_ref_pic = sps->offset_for_non_ref_pic;
	vsps->offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field;
	vsps->pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1;
	vsps->pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1;
	vsps->flags = 0;
	if (sps->separate_colour_plane_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_SEPARATE_COLOUR_PLANE;
	if (sps->qpprime_y_zero_transform_bypass_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS;
	if (sps->delta_pic_order_always_zero_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO;
	if (sps->gaps_in_frame_num_value_allowed_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_GAPS_IN_FRAME_NUM_VALUE_ALLOWED;
	if (sps->frame_mbs_only_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY;
	if (sps->mb_adaptive_frame_field_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD;
	if (sps->direct_8x8_inference_flag)
		vsps->flags |= V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE;

	/* PPS */
	vpps->pic_parameter_set_id = pps->pic_parameter_set_id;
	vpps->seq_parameter_set_id = pps->seq_parameter_set_id;
	vpps->num_slice_groups_minus1 = pps->num_slice_groups_minus1;
	vpps->num_ref_idx_l0_default_active_minus1 =
		pps->num_ref_idx_l0_default_active_minus1;
	vpps->num_ref_idx_l1_default_active_minus1 =
		pps->num_ref_idx_l1_default_active_minus1;
	vpps->weighted_bipred_idc = pps->weighted_bipred_idc;
	vpps->pic_init_qp_minus26 = pps->pic_init_qp_minus26;
	vpps->pic_init_qs_minus26 = pps->pic_init_qs_minus26;
	vpps->chroma_qp_index_offset = pps->chroma_qp_index_offset;
	vpps->second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;
	vpps->flags = 0;
	if (pps->entropy_coding_mode_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE;
	if (pps->bottom_field_pic_order_in_frame_present_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT;
	if (pps->weighted_pred_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_WEIGHTED_PRED;
	if (pps->deblocking_filter_control_present_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT;
	if (pps->constrained_intra_pred_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED;
	if (pps->redundant_pic_cnt_present_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT;
	if (pps->transform_8x8_mode_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE;
	if (pps->pic_scaling_matrix_present_flag ||
	    sps->seq_scaling_matrix_present_flag)
		vpps->flags |= V4L2_H264_PPS_FLAG_PIC_SCALING_MATRIX_PRESENT;

	/*
	 * Scaling matrix (raster order). Prefer the PPS lists, else the SPS
	 * lists, else flat 16 (the spec's Flat_4x4_16 / Flat_8x8_16 default
	 * when no scaling matrix is signalled).
	 */
	if (pps->pic_scaling_matrix_present_flag) {
		memcpy(ctrl->scaling_matrix.scaling_list_4x4,
		       pps->scaling_list_4x4,
		       sizeof(ctrl->scaling_matrix.scaling_list_4x4));
		memcpy(ctrl->scaling_matrix.scaling_list_8x8,
		       pps->scaling_list_8x8,
		       sizeof(ctrl->scaling_matrix.scaling_list_8x8));
	} else if (sps->seq_scaling_matrix_present_flag) {
		memcpy(ctrl->scaling_matrix.scaling_list_4x4,
		       sps->scaling_list_4x4,
		       sizeof(ctrl->scaling_matrix.scaling_list_4x4));
		memcpy(ctrl->scaling_matrix.scaling_list_8x8,
		       sps->scaling_list_8x8,
		       sizeof(ctrl->scaling_matrix.scaling_list_8x8));
	} else {
		memset(&ctrl->scaling_matrix, 16,
		       sizeof(ctrl->scaling_matrix));
	}

	/* Prediction weights: 5.4 uapi 内联在 slice_params，无独立控制 */
	if (h->has_pred_weights) {
		vsl->pred_weight_table.luma_log2_weight_denom =
			h->luma_log2_weight_denom;
		vsl->pred_weight_table.chroma_log2_weight_denom =
			h->chroma_log2_weight_denom;
		vsl->pred_weight_table.weight_factors[0] = h->weights[0];
		vsl->pred_weight_table.weight_factors[1] = h->weights[1];
	}

	/* Slice params. 5.4: frame_num/POC lsb/marking 位数等都在这里，
	 * 不在 decode_params（stable uapi 把它们搬走了，勿混）。
	 * size 由引擎按提交的 NAL 大小填；start_byte_offset 恒 0。 */
	vsl->header_bit_size = 8 * 1 /* nal header byte */ +
		h->header_size - 8 * h->n_emulation_prevention_bytes;
	vsl->first_mb_in_slice = h->first_mb_in_slice;
	vsl->slice_type = h->slice_type % 5;
	vsl->pic_parameter_set_id = h->pic_parameter_set_id;
	vsl->colour_plane_id = h->colour_plane_id;
	vsl->redundant_pic_cnt = h->redundant_pic_cnt;
	vsl->frame_num = h->frame_num;
	vsl->idr_pic_id = h->idr_pic_id;
	vsl->pic_order_cnt_lsb = h->pic_order_cnt_lsb;
	vsl->delta_pic_order_cnt_bottom = h->delta_pic_order_cnt_bottom;
	vsl->delta_pic_order_cnt0 = h->delta_pic_order_cnt[0];
	vsl->delta_pic_order_cnt1 = h->delta_pic_order_cnt[1];
	vsl->dec_ref_pic_marking_bit_size = h->dec_ref_pic_marking_bit_size;
	vsl->pic_order_cnt_bit_size = h->pic_order_cnt_bit_size;
	vsl->cabac_init_idc = h->cabac_init_idc;
	vsl->slice_qp_delta = h->slice_qp_delta;
	vsl->slice_qs_delta = h->slice_qs_delta;
	vsl->disable_deblocking_filter_idc = h->disable_deblocking_filter_idc;
	vsl->slice_alpha_c0_offset_div2 = h->slice_alpha_c0_offset_div2;
	vsl->slice_beta_offset_div2 = h->slice_beta_offset_div2;
	vsl->num_ref_idx_l0_active_minus1 = h->num_ref_idx_l0_active_minus1;
	vsl->num_ref_idx_l1_active_minus1 = h->num_ref_idx_l1_active_minus1;
	vsl->flags = 0;
	if (h->field_pic_flag)
		vsl->flags |= V4L2_H264_SLICE_FLAG_FIELD_PIC;
	if (h->bottom_field_flag)
		vsl->flags |= V4L2_H264_SLICE_FLAG_BOTTOM_FIELD;
	if (h->direct_spatial_mv_pred_flag)
		vsl->flags |= V4L2_H264_SLICE_FLAG_DIRECT_SPATIAL_MV_PRED;
	if (h->sp_for_switch_flag)
		vsl->flags |= V4L2_H264_SLICE_FLAG_SP_FOR_SWITCH;
	/* ref_pic_list0/1 filled by the DPB layer. */

	/* Decode params: 5.4 只剩 dpb/num_slices/nal_ref_idc/POC/flags */
	vdp->nal_ref_idc = h->nal_ref_idc;
	vdp->num_slices = 1;
	vdp->top_field_order_cnt = poc->top_field_order_cnt;
	vdp->bottom_field_order_cnt = poc->bottom_field_order_cnt;
	vdp->flags = 0;
	if (h->idr)
		vdp->flags |= V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC;
	/* dpb[] filled by the DPB layer. */

	return 0;
}
