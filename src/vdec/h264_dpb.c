/*
 * H.264 DPB, reference lists and display reordering. See h264_dpb.h.
 * Frame-only (no fields/MBAFF). Sliding window + MMCO 1..6.
 */

#include "h264_dpb.h"

#include <string.h>

void h264_dpb_init(struct h264_dpb *dpb, int capacity, int max_num_ref_frames,
		   int max_frame_num, int reorder_depth)
{
	memset(dpb, 0, sizeof(*dpb));
	dpb->capacity = capacity > H264_DPB_MAX_SLOTS ? H264_DPB_MAX_SLOTS :
		capacity;
	dpb->max_num_ref_frames = max_num_ref_frames < 1 ? 1 : max_num_ref_frames;
	dpb->max_frame_num = max_frame_num;
	dpb->reorder_depth = reorder_depth;
	dpb->max_long_term_frame_idx = -1;
	dpb->ts_counter = 0;
	dpb->cur = -1;
}

static void free_if_done(struct h264_dpb_pic *p)
{
	if (!p->in_dpb && !p->needed_for_output && !p->on_screen)
		p->used = false;
}

static int find_free_slot(struct h264_dpb *dpb)
{
	int i;
	for (i = 0; i < dpb->capacity; i++)
		if (!dpb->pics[i].used)
			return i;
	return -1;
}

/* ---- reference list construction helpers ---- */

/* Sort an index list in place by a key, descending (dir<0) or ascending. */
static void sort_list(struct h264_dpb *dpb, int *list, int n,
		      bool by_poc, bool ascending)
{
	int i, j;
	for (i = 1; i < n; i++) {
		int key = list[i];
		int kv = by_poc ? dpb->pics[key].poc : dpb->pics[key].pic_num;
		j = i - 1;
		while (j >= 0) {
			int cur = by_poc ? dpb->pics[list[j]].poc :
				dpb->pics[list[j]].pic_num;
			bool swap = ascending ? (cur > kv) : (cur < kv);
			if (!swap)
				break;
			list[j + 1] = list[j];
			j--;
		}
		list[j + 1] = key;
	}
}

static int collect_short_term(struct h264_dpb *dpb, int *out)
{
	int i, n = 0;
	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref && !p->long_term)
			out[n++] = i;
	}
	return n;
}

static int collect_long_term(struct h264_dpb *dpb, int *out)
{
	int i, n = 0;
	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref && p->long_term)
			out[n++] = i;
	}
	return n;
}

static void sort_long_term(struct h264_dpb *dpb, int *list, int n)
{
	int i, j;
	for (i = 1; i < n; i++) {
		int key = list[i];
		int kv = dpb->pics[key].long_term_frame_idx;
		j = i - 1;
		while (j >= 0 &&
		       dpb->pics[list[j]].long_term_frame_idx > kv) {
			list[j + 1] = list[j];
			j--;
		}
		list[j + 1] = key;
	}
}

/* Compute FrameNumWrap / PicNum for all short-term refs vs current frame_num. */
static void compute_pic_nums(struct h264_dpb *dpb, int cur_frame_num)
{
	int i;
	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (!(p->used && p->in_dpb && p->is_ref))
			continue;
		if (p->long_term) {
			p->pic_num = p->long_term_frame_idx;
		} else {
			if (p->frame_num > cur_frame_num)
				p->frame_num_wrap = p->frame_num - dpb->max_frame_num;
			else
				p->frame_num_wrap = p->frame_num;
			p->pic_num = p->frame_num_wrap;
		}
	}
}

static int build_list_p(struct h264_dpb *dpb, int *list)
{
	int st[H264_DPB_MAX_SLOTS], lt[H264_DPB_MAX_SLOTS];
	int nst, nlt, n = 0, i;

	nst = collect_short_term(dpb, st);
	nlt = collect_long_term(dpb, lt);
	sort_list(dpb, st, nst, false, false);	/* PicNum descending */
	sort_long_term(dpb, lt, nlt);
	for (i = 0; i < nst; i++)
		list[n++] = st[i];
	for (i = 0; i < nlt; i++)
		list[n++] = lt[i];
	return n;
}

static int build_list_b(struct h264_dpb *dpb, int32_t cur_poc, int which,
			int *list)
{
	int before[H264_DPB_MAX_SLOTS], after[H264_DPB_MAX_SLOTS];
	int lt[H264_DPB_MAX_SLOTS];
	int nb = 0, na = 0, nlt, n = 0, i;
	int st[H264_DPB_MAX_SLOTS];
	int nst = collect_short_term(dpb, st);

	for (i = 0; i < nst; i++) {
		if (dpb->pics[st[i]].poc < cur_poc)
			before[nb++] = st[i];
		else
			after[na++] = st[i];
	}
	/* before: descending POC; after: ascending POC */
	sort_list(dpb, before, nb, true, false);
	sort_list(dpb, after, na, true, true);
	nlt = collect_long_term(dpb, lt);
	sort_long_term(dpb, lt, nlt);

	if (which == 0) {
		for (i = 0; i < nb; i++) list[n++] = before[i];
		for (i = 0; i < na; i++) list[n++] = after[i];
	} else {
		for (i = 0; i < na; i++) list[n++] = after[i];
		for (i = 0; i < nb; i++) list[n++] = before[i];
	}
	for (i = 0; i < nlt; i++)
		list[n++] = lt[i];
	return n;
}

static int find_short_by_picnum(struct h264_dpb *dpb, int pic_num)
{
	int i;
	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref && !p->long_term &&
		    p->pic_num == pic_num)
			return i;
	}
	return -1;
}

static int find_long_by_ltpn(struct h264_dpb *dpb, int ltpn)
{
	int i;
	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref && p->long_term &&
		    p->long_term_frame_idx == ltpn)
			return i;
	}
	return -1;
}

/* Apply ref_pic_list_modification (8.2.4.3) to a pool-index list. */
static void modify_list(struct h264_dpb *dpb, int *list, int num_active,
			const struct h264_rplm *rplm, int n_rplm,
			int cur_pic_num)
{
	int max_pic_num = dpb->max_frame_num;
	int pic_num_pred = cur_pic_num;
	int refidx = 0, i, m;

	for (m = 0; m < n_rplm; m++) {
		int target = -1;

		if (rplm[m].idc == 0 || rplm[m].idc == 1) {
			int abs_diff = rplm[m].val + 1;
			int pic_num;
			if (rplm[m].idc == 0) {
				pic_num = pic_num_pred - abs_diff;
				if (pic_num < 0)
					pic_num += max_pic_num;
			} else {
				pic_num = pic_num_pred + abs_diff;
				if (pic_num >= max_pic_num)
					pic_num -= max_pic_num;
			}
			pic_num_pred = pic_num;
			/* pic_num may exceed cur_pic_num -> wrap to signed PicNum */
			if (pic_num > cur_pic_num)
				pic_num -= max_pic_num;
			target = find_short_by_picnum(dpb, pic_num);
		} else if (rplm[m].idc == 2) {
			target = find_long_by_ltpn(dpb, rplm[m].val);
		}

		if (target < 0 || refidx >= num_active)
			continue;

		/* shift down, insert target at refidx, drop later duplicate */
		for (i = num_active - 1; i > refidx; i--)
			list[i] = list[i - 1];
		list[refidx] = target;
		i = refidx + 1;
		{
			int src;
			for (src = refidx + 1; src <= num_active; src++) {
				if (src < num_active && list[src] == target)
					continue;
				if (i <= num_active - 1 && src < num_active)
					list[i++] = list[src];
			}
		}
		refidx++;
	}
}

/*
 * 重排后的列表长度恒为 num_active(8.2.4.3)：x264 weightp 常用
 * "同一参考列两次配不同权重"，初始列表(物理参考数)比 active 短，
 * 重排负责填满——之前把重排前的长度传给 fill_ref_list，尾项被
 * 悄悄丢成 dpb[0]，P 帧 ref_idx 末位全指错帧(拖尾根因)。
 * 初始列表之外的位置先置 -1，防 modify_list 的移位搬进未初始化值。
 */
static int modified_len(struct h264_dpb *dpb, int *list, int n_init,
			int num_active, const struct h264_rplm *rplm,
			int n_rplm, int cur_pic_num)
{
	int i;

	if (num_active > H264_DPB_MAX_SLOTS)
		num_active = H264_DPB_MAX_SLOTS;
	for (i = n_init; i < num_active; i++)
		list[i] = -1;
	modify_list(dpb, list, num_active, rplm, n_rplm, cur_pic_num);
	return num_active;
}

/* ---- control filling ---- */

static int build_dpb_control(struct h264_dpb *dpb, struct vdec_h264_ctrls *ctrl,
			     int *map)
{
	struct v4l2_ctrl_h264_decode_params *vdp = &ctrl->decode_params;
	int i, n = 0;

	for (i = 0; i < dpb->capacity; i++)
		map[i] = -1;

	for (i = 0; i < dpb->capacity && n < 16; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (!(p->used && p->in_dpb && p->is_ref))
			continue;
		vdp->dpb[n].reference_ts = p->ts;
		vdp->dpb[n].frame_num = p->frame_num;
		/* 5.4: pic_num 是 __u16，cedrus 不读，负 PicNum 截断无害 */
		vdp->dpb[n].pic_num = p->long_term ?
			p->long_term_frame_idx : p->pic_num;
		vdp->dpb[n].top_field_order_cnt = p->top_poc;
		vdp->dpb[n].bottom_field_order_cnt = p->bottom_poc;
		vdp->dpb[n].flags = V4L2_H264_DPB_ENTRY_FLAG_VALID |
				    V4L2_H264_DPB_ENTRY_FLAG_ACTIVE |
				    (p->long_term ?
				     V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM : 0);
		map[i] = n;
		n++;
	}
	return n;
}

/* 5.4: ref list 是裸 __u8 dpb 下标数组（stable uapi 才有 {fields,index}） */
static void fill_ref_list(__u8 *out, const int *list,
			  int len, const int *map, int num_active)
{
	int k;
	memset(out, 0, 32);
	for (k = 0; k < num_active && k < 32; k++) {
		if (k < len && list[k] >= 0 && map[list[k]] >= 0)
			out[k] = map[list[k]];
	}
}

int h264_dpb_begin_frame(struct h264_dpb *dpb, const struct h264_slice_hdr *hdr,
			 const struct h264_poc *poc, uint64_t *ts_out,
			 struct vdec_h264_ctrls *ctrl)
{
	int map[H264_DPB_MAX_SLOTS];
	int list0[H264_DPB_MAX_SLOTS], list1[H264_DPB_MAX_SLOTS];
	int n0 = 0, n1 = 0;
	int slot, i;
	uint8_t st = hdr->slice_type % 5;

	/* IDR clears all references before building (empty DPB for this pic). */
	if (hdr->idr) {
		for (i = 0; i < dpb->capacity; i++) {
			dpb->pics[i].in_dpb = false;
			free_if_done(&dpb->pics[i]);
		}
		dpb->max_long_term_frame_idx = -1;
	}

	compute_pic_nums(dpb, hdr->frame_num);
	build_dpb_control(dpb, ctrl, map);

	if (st == H264_SLICE_P || st == H264_SLICE_SP) {
		n0 = build_list_p(dpb, list0);
		if (hdr->ref_pic_list_modification_flag_l0 && n0)
			n0 = modified_len(dpb, list0, n0,
					  hdr->num_ref_idx_l0_active_minus1 + 1,
					  hdr->rplm_l0, hdr->n_rplm_l0,
					  hdr->frame_num);
		fill_ref_list(ctrl->slice_params.ref_pic_list0, list0, n0,
			      map, hdr->num_ref_idx_l0_active_minus1 + 1);
	} else if (st == H264_SLICE_B) {
		n0 = build_list_b(dpb, poc->poc, 0, list0);
		n1 = build_list_b(dpb, poc->poc, 1, list1);
		/* if L1 == L0 and has >1 entry, swap first two of L1 */
		if (n1 > 1) {
			bool same = (n0 == n1);
			for (i = 0; same && i < n0; i++)
				if (list0[i] != list1[i])
					same = false;
			if (same) {
				int t = list1[0];
				list1[0] = list1[1];
				list1[1] = t;
			}
		}
		if (hdr->ref_pic_list_modification_flag_l0 && n0)
			n0 = modified_len(dpb, list0, n0,
					  hdr->num_ref_idx_l0_active_minus1 + 1,
					  hdr->rplm_l0, hdr->n_rplm_l0,
					  hdr->frame_num);
		if (hdr->ref_pic_list_modification_flag_l1 && n1)
			n1 = modified_len(dpb, list1, n1,
					  hdr->num_ref_idx_l1_active_minus1 + 1,
					  hdr->rplm_l1, hdr->n_rplm_l1,
					  hdr->frame_num);
		fill_ref_list(ctrl->slice_params.ref_pic_list0, list0, n0,
			      map, hdr->num_ref_idx_l0_active_minus1 + 1);
		fill_ref_list(ctrl->slice_params.ref_pic_list1, list1, n1,
			      map, hdr->num_ref_idx_l1_active_minus1 + 1);
	}

	slot = find_free_slot(dpb);
	if (slot < 0)
		return -1;

	dpb->pics[slot].used = true;
	dpb->pics[slot].slot = slot;
	dpb->pics[slot].ts = (++dpb->ts_counter) * 1000;
	dpb->pics[slot].frame_num = hdr->frame_num;
	dpb->pics[slot].top_poc = poc->top_field_order_cnt;
	dpb->pics[slot].bottom_poc = poc->bottom_field_order_cnt;
	dpb->pics[slot].poc = poc->poc;
	dpb->pics[slot].is_ref = (hdr->nal_ref_idc != 0);
	dpb->pics[slot].long_term = false;
	dpb->pics[slot].long_term_frame_idx = 0;
	dpb->pics[slot].in_dpb = false;
	dpb->pics[slot].needed_for_output = true;
	dpb->pics[slot].on_screen = false;
	dpb->cur = slot;

	*ts_out = dpb->pics[slot].ts;
	return slot;
}

static void sliding_window(struct h264_dpb *dpb)
{
	int i, count = 0, victim = -1, min_wrap = 0;

	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref)
			count++;
	}
	if (count < dpb->max_num_ref_frames)
		return;

	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->in_dpb && p->is_ref && !p->long_term) {
			if (victim < 0 || p->frame_num_wrap < min_wrap) {
				victim = i;
				min_wrap = p->frame_num_wrap;
			}
		}
	}
	if (victim >= 0) {
		dpb->pics[victim].in_dpb = false;
		free_if_done(&dpb->pics[victim]);
	}
}

static void apply_mmco(struct h264_dpb *dpb, const struct h264_slice_hdr *hdr)
{
	int cur_pic_num = hdr->frame_num;
	int i, k;

	for (k = 0; k < hdr->n_mmco; k++) {
		const struct h264_mmco *m = &hdr->mmco[k];
		switch (m->op) {
		case 1: {
			int picnum = cur_pic_num - (int)(m->arg1 + 1);
			int idx = find_short_by_picnum(dpb, picnum);
			if (idx >= 0) {
				dpb->pics[idx].in_dpb = false;
				free_if_done(&dpb->pics[idx]);
			}
			break;
		}
		case 2: {
			int idx = find_long_by_ltpn(dpb, m->arg1);
			if (idx >= 0) {
				dpb->pics[idx].in_dpb = false;
				free_if_done(&dpb->pics[idx]);
			}
			break;
		}
		case 3: {
			int picnum = cur_pic_num - (int)(m->arg1 + 1);
			int idx = find_short_by_picnum(dpb, picnum);
			int old = find_long_by_ltpn(dpb, m->arg2);
			if (old >= 0) {
				dpb->pics[old].in_dpb = false;
				free_if_done(&dpb->pics[old]);
			}
			if (idx >= 0) {
				dpb->pics[idx].long_term = true;
				dpb->pics[idx].long_term_frame_idx = m->arg2;
			}
			break;
		}
		case 4: {
			int max_lt = (int)m->arg1 - 1;
			dpb->max_long_term_frame_idx = max_lt;
			for (i = 0; i < dpb->capacity; i++) {
				struct h264_dpb_pic *p = &dpb->pics[i];
				if (p->used && p->in_dpb && p->is_ref &&
				    p->long_term &&
				    p->long_term_frame_idx > max_lt) {
					p->in_dpb = false;
					free_if_done(p);
				}
			}
			break;
		}
		case 5:
			for (i = 0; i < dpb->capacity; i++) {
				if (dpb->pics[i].used && dpb->pics[i].in_dpb) {
					dpb->pics[i].in_dpb = false;
					free_if_done(&dpb->pics[i]);
				}
			}
			dpb->max_long_term_frame_idx = -1;
			break;
		case 6:
			if (dpb->cur >= 0) {
				dpb->pics[dpb->cur].long_term = true;
				dpb->pics[dpb->cur].long_term_frame_idx = m->arg2;
			}
			break;
		default:
			break;
		}
	}
}

void h264_dpb_end_frame(struct h264_dpb *dpb, const struct h264_slice_hdr *hdr)
{
	struct h264_dpb_pic *cur;

	if (dpb->cur < 0)
		return;
	cur = &dpb->pics[dpb->cur];

	if (!cur->is_ref) {
		dpb->cur = -1;
		return;
	}

	if (hdr->idr) {
		if (hdr->long_term_reference_flag) {
			cur->long_term = true;
			cur->long_term_frame_idx = 0;
			dpb->max_long_term_frame_idx = 0;
		}
		cur->in_dpb = true;
	} else if (hdr->adaptive_ref_pic_marking_mode_flag) {
		apply_mmco(dpb, hdr);
		cur->in_dpb = true;
	} else {
		sliding_window(dpb);
		cur->in_dpb = true;
	}

	dpb->cur = -1;
}

void h264_dpb_abort_frame(struct h264_dpb *dpb)
{
	if (dpb->cur < 0)
		return;
	dpb->pics[dpb->cur].needed_for_output = false;
	dpb->pics[dpb->cur].in_dpb = false;
	dpb->pics[dpb->cur].used = false;
	dpb->cur = -1;
}

int h264_dpb_next_output(struct h264_dpb *dpb, bool flush)
{
	int i, count = 0, best = -1;
	int32_t best_poc = 0;

	for (i = 0; i < dpb->capacity; i++)
		if (dpb->pics[i].used && dpb->pics[i].needed_for_output)
			count++;

	if (count == 0)
		return -1;
	if (!flush && count <= dpb->reorder_depth)
		return -1;

	for (i = 0; i < dpb->capacity; i++) {
		struct h264_dpb_pic *p = &dpb->pics[i];
		if (p->used && p->needed_for_output) {
			if (best < 0 || p->poc < best_poc) {
				best = i;
				best_poc = p->poc;
			}
		}
	}
	return best;
}

void h264_dpb_mark_displayed(struct h264_dpb *dpb, int slot)
{
	if (slot < 0 || slot >= dpb->capacity)
		return;
	dpb->pics[slot].needed_for_output = false;
	free_if_done(&dpb->pics[slot]);
}

void h264_dpb_set_on_screen(struct h264_dpb *dpb, int slot, bool on)
{
	if (slot < 0 || slot >= dpb->capacity)
		return;
	dpb->pics[slot].on_screen = on;
	if (!on)
		free_if_done(&dpb->pics[slot]);
}
