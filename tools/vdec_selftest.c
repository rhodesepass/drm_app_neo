/*
 * vdec_selftest — src/vdec 解码栈的独立验证工具（M0，不依赖 app 其余部分）。
 *
 * 用法:
 *   vdec_selftest -H file.mp4          宿主机可跑: 解析头 + DPB 重排仿真
 *   vdec_selftest file.mp4             真机: 解码并经 DRM plane 上屏
 *     -f fps     覆盖 MP4 内的帧率定速
 *     -p idx     指定 plane 序号 (默认 0 = video 层)
 *     -w n       n 帧原始 tiled NV12 写到 /tmp/vdec_frame_%u.raw
 *     -s n       dump 从解码序第 n 帧开始 (配合 -w 取后段窗口)
 *     -n n       只解前 n 帧
 *
 * 解码路径同 mediaplayer 的契约: 单 slice/帧、START_CODE_NONE 裸 NAL、
 * capture slot == DPB slot、dmabuf 导入 DRM FB (NV12 + ALLWINNER_TILED)。
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "mp4_demux.h"
#include "nalu.h"
#include "h264_parser.h"
#include "h264_dpb.h"

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((unsigned int)(a) - 1))

static const char *slice_type_str(uint8_t t)
{
	switch (t % 5) {
	case H264_SLICE_P: return "P";
	case H264_SLICE_B: return "B";
	case H264_SLICE_I: return "I";
	case H264_SLICE_SP: return "SP";
	case H264_SLICE_SI: return "SI";
	default: return "?";
	}
}

static void h264_dimensions(const struct h264_sps *sps, unsigned int *w,
			    unsigned int *h)
{
	*w = (sps->pic_width_in_mbs_minus1 + 1) * 16;
	*h = (sps->pic_height_in_map_units_minus1 + 1) * 16 *
	     (sps->frame_mbs_only_flag ? 1 : 2);
}

static const struct h264_sps *first_sps(struct h264_parser *p)
{
	int i;

	for (i = 0; i < 32; i++) {
		const struct h264_sps *s = h264_parser_get_sps(p, i);
		if (s)
			return s;
	}
	return NULL;
}

/* ---- host mode: header dump + display-order simulation ---- */

static int host_check(struct mp4_demux *m)
{
	struct h264_parser parser;
	struct h264_dpb dpb;
	const struct h264_sps *sps0;
	unsigned int max_ref = 4, max_frame_num = 16, i;
	int out, prev_poc = -1000000, gop_ok = 1, count = 0;
	/* 输出序地面真值：slot -> 喂入时的解码序号，OUT 行给外部脚本
	 * 对照 ffprobe 的 pts 表验证显示序(POC 自检是自我指涉，抓不到
	 * POC 本身算错) */
	int slot2idx[64];

	memset(slot2idx, -1, sizeof(slot2idx));

	h264_parser_init(&parser);
	if (h264_parser_parse_avcc(&parser, m->extradata, m->extradata_size) < 0)
		fprintf(stderr, "warning: failed to parse avcC\n");

	sps0 = first_sps(&parser);
	if (sps0) {
		max_ref = sps0->max_num_ref_frames ? sps0->max_num_ref_frames : 1;
		max_frame_num = 1 << (sps0->log2_max_frame_num_minus4 + 4);
	}
	/* 重排深度与 mediaplayer 完全一致(VUI 优先)，否则仿真结果对不上真机 */
	{
		unsigned int reorder = max_ref < 2 ? 2 : max_ref;

		if (sps0 && sps0->vui_reorder_valid)
			reorder = sps0->vui_max_num_reorder_frames;
		printf("sim reorder_depth=%u (vui_valid=%d)\n", reorder,
		       sps0 ? sps0->vui_reorder_valid : -1);
		h264_dpb_init(&dpb, max_ref * 2 + 5, max_ref, max_frame_num,
			      reorder);
	}

	printf("codec=H264 length_size=%u samples=%u %ux%u dur=%uus max_sample=%u\n",
	       m->nal_length_size, m->samples_count, m->width, m->height,
	       m->frame_duration_us, m->max_sample_size);

	for (i = 0; i < m->samples_count; i++) {
		const uint8_t *au = NULL;
		unsigned int cursor = 0, vcl_count = 0;
		struct nalu n;
		struct h264_slice_hdr hdr;
		struct h264_poc poc;
		struct vdec_h264_ctrls ctrl;
		bool have = false;
		uint64_t ts;
		int slot;

		if (mp4_read_sample(m, i, &au, NULL) != MP4_OK || !au)
			break;

		while (nalu_next_length_prefixed(au, m->samples[i].size,
						 m->nal_length_size, &cursor, &n)) {
			unsigned int t = nalu_h264_type(&n);

			if (t == H264_NAL_SPS || t == H264_NAL_PPS) {
				h264_parser_parse_param_nal(&parser, &n);
			} else if (t == H264_NAL_SLICE || t == H264_NAL_IDR) {
				vcl_count++;
				if (have)
					continue;
				if (h264_parser_parse_slice(&parser, &n, &hdr) < 0) {
					printf("  [%u] slice parse failed\n", i);
					break;
				}
				h264_parser_compute_poc(&parser, &hdr, &poc);
				have = true;
			}
		}
		if (!have)
			continue;
		if (vcl_count > 1)
			printf("  [%4u] WARNING: %u slices per frame "
			       "(mediaplayer will reject)\n", i, vcl_count);

		if (i < 32 || hdr.idr)
			printf("  [%4u] %-2s %s frame_num=%u poc=%d "
			       "hdr_bits(raw=%u epb=%u) size=%u%s\n",
			       i, slice_type_str(hdr.slice_type),
			       hdr.idr ? "IDR" : "   ", hdr.frame_num, poc.poc,
			       hdr.header_size, hdr.n_emulation_prevention_bytes,
			       m->samples[i].size,
			       m->samples[i].sync ? " sync" : "");

		if (hdr.idr) {
			while ((out = h264_dpb_next_output(&dpb, true)) >= 0) {
				if (dpb.pics[out].poc < prev_poc)
					gop_ok = 0;
				prev_poc = dpb.pics[out].poc;
				count++;
				printf("OUT %d\n", slot2idx[out]);
				h264_dpb_mark_displayed(&dpb, out);
			}
			prev_poc = -1000000; /* POC 每 IDR GOP 复位 */
		}
		if (h264_parser_fill_controls(&parser, &hdr, &poc, &ctrl) < 0)
			continue;
		slot = h264_dpb_begin_frame(&dpb, &hdr, &poc, &ts, &ctrl);
		if (slot < 0) {
			printf("  SIM: slot exhausted @%u\n", i);
			break;
		}
		slot2idx[slot] = (int)i;
		h264_dpb_end_frame(&dpb, &hdr);
		while ((out = h264_dpb_next_output(&dpb, false)) >= 0) {
			if (dpb.pics[out].poc < prev_poc)
				gop_ok = 0;
			prev_poc = dpb.pics[out].poc;
			count++;
			printf("OUT %d\n", slot2idx[out]);
			h264_dpb_mark_displayed(&dpb, out);
		}
	}
	while ((out = h264_dpb_next_output(&dpb, true)) >= 0) {
		if (dpb.pics[out].poc < prev_poc)
			gop_ok = 0;
		prev_poc = dpb.pics[out].poc;
		count++;
		printf("OUT %d\n", slot2idx[out]);
		h264_dpb_mark_displayed(&dpb, out);
	}
	printf("--- simulation: %d frames output, per-GOP POC monotonic: %s ---\n",
	       count, gop_ok ? "YES" : "NO");
	return gop_ok ? 0 : 1;
}

#ifndef VDEC_SELFTEST_HOST_ONLY

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "vdec_v4l2.h"

#ifndef DRM_FORMAT_MOD_ALLWINNER_TILED
#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)
#endif

struct display {
	int fd;
	uint32_t crtc_id;
	uint32_t plane_id;
	unsigned int screen_w, screen_h;
	uint32_t fb_ids[VDEC_MAX_CAP_BUFS];
};

static int display_open(struct display *d, struct vdec_ctx *v, int plane_index)
{
	drmModeRes *res;
	drmModePlaneRes *planes;
	drmModeCrtc *crtc;
	unsigned int i;

	memset(d, 0, sizeof(*d));
	d->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (d->fd < 0) {
		fprintf(stderr, "open /dev/dri/card0: %s\n", strerror(errno));
		return -1;
	}
	drmSetClientCap(d->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

	res = drmModeGetResources(d->fd);
	if (!res || !res->count_crtcs)
		return -1;
	d->crtc_id = res->crtcs[0];
	crtc = drmModeGetCrtc(d->fd, d->crtc_id);
	if (crtc) {
		d->screen_w = crtc->mode.hdisplay;
		d->screen_h = crtc->mode.vdisplay;
		drmModeFreeCrtc(crtc);
	}
	drmModeFreeResources(res);

	planes = drmModeGetPlaneResources(d->fd);
	if (!planes || (unsigned int)plane_index >= planes->count_planes) {
		fprintf(stderr, "bad plane index %d\n", plane_index);
		return -1;
	}
	d->plane_id = planes->planes[plane_index];
	drmModeFreePlaneResources(planes);

	for (i = 0; i < v->cap_count; i++) {
		uint32_t handle = 0;
		uint32_t handles[4] = { 0 }, pitches[4] = { 0 },
			 offsets[4] = { 0 };
		uint64_t modifiers[4] = { 0 };

		if (drmPrimeFDToHandle(d->fd, v->cap[i].dmabuf_fd, &handle) < 0) {
			fprintf(stderr, "PrimeFDToHandle[%u]: %s\n", i,
				strerror(errno));
			return -1;
		}
		handles[0] = handles[1] = handle;
		pitches[0] = pitches[1] = v->cap_bytesperline;
		offsets[0] = 0;
		offsets[1] = v->cap_uv_offset;
		modifiers[0] = modifiers[1] = DRM_FORMAT_MOD_ALLWINNER_TILED;

		if (drmModeAddFB2WithModifiers(d->fd, v->cap_width,
					       v->cap_height, DRM_FORMAT_NV12,
					       handles, pitches, offsets,
					       modifiers, &d->fb_ids[i],
					       DRM_MODE_FB_MODIFIERS) < 0) {
			fprintf(stderr, "AddFB2[%u]: %s\n", i, strerror(errno));
			return -1;
		}
	}

	printf("display: plane %u crtc %u screen %ux%u\n", d->plane_id,
	       d->crtc_id, d->screen_w, d->screen_h);
	return 0;
}

static int display_show(struct display *d, struct vdec_ctx *v, int slot)
{
	unsigned int src_w = v->cap_width, src_h = v->cap_height;

	/* 裁掉右侧 32 对齐 padding，不越过屏幕 */
	if (d->screen_w && src_w > d->screen_w)
		src_w = d->screen_w;
	if (d->screen_h && src_h > d->screen_h)
		src_h = d->screen_h;

	return drmModeSetPlane(d->fd, d->plane_id, d->crtc_id, d->fb_ids[slot],
			       0, 0, 0, src_w, src_h, 0, 0, src_w << 16,
			       src_h << 16);
}

static void display_close(struct display *d, struct vdec_ctx *v)
{
	unsigned int i;

	if (d->fd < 0)
		return;
	for (i = 0; i < v->cap_count; i++)
		if (d->fb_ids[i])
			drmModeRmFB(d->fd, d->fb_ids[i]);
	close(d->fd);
}

static long long now_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void dump_frame(struct vdec_ctx *v, int slot, unsigned int seq)
{
	char path[64];
	FILE *f;

	snprintf(path, sizeof(path), "/tmp/vdec_frame_%u.raw", seq);
	f = fopen(path, "wb");
	if (!f)
		return;
	fwrite(v->cap[slot].map, 1, v->cap_sizeimage, f);
	fclose(f);
	printf("dumped %s (%u bytes, tiled NV12 %ux%u bpl=%u)\n", path,
	       v->cap_sizeimage, v->cap_width, v->cap_height,
	       v->cap_bytesperline);
}

static int device_decode(struct mp4_demux *m, int fps_override, int plane_index,
			 unsigned int dump_n, unsigned int dump_start,
			 unsigned int limit)
{
	struct h264_parser parser;
	struct h264_dpb dpb;
	struct vdec_ctx vdec;
	struct display disp;
	const struct h264_sps *sps0;
	char video_path[32], media_path[32];
	unsigned int width, height, cap_count, max_ref, max_frame_num = 16;
	unsigned int reorder_depth = 2;
	unsigned int i, decode_count = 0, display_count = 0;
	unsigned int frame_dur;
	long long decode_us_total = 0, next_frame = 0;
	int rc = -1, out;
	int held[2] = { -1, -1 };	/* 在屏帧 + 押一拍的上一帧 */

	h264_parser_init(&parser);
	h264_parser_parse_avcc(&parser, m->extradata, m->extradata_size);

	sps0 = first_sps(&parser);
	if (!sps0) {
		fprintf(stderr, "no SPS in avcC\n");
		return -1;
	}
	h264_dimensions(sps0, &width, &height);
	width = ALIGN_UP(width, 32);
	height = ALIGN_UP(height, 32);
	max_frame_num = 1 << (sps0->log2_max_frame_num_minus4 + 4);
	max_ref = sps0->max_num_ref_frames ? sps0->max_num_ref_frames : 1;

	{
		/* 与 mediaplayer 同账：VUI 有 DPB 联合上限就用精确账 */
		unsigned int reorder = sps0->vui_reorder_valid ?
			sps0->vui_max_num_reorder_frames :
			(max_ref < 2 ? 2 : max_ref);
		unsigned int cap_max = width * height >= 600 * 1000 ? 9 : 16;

		cap_count = sps0->vui_reorder_valid ?
			(unsigned int)sps0->vui_max_dec_frame_buffering + 5 :
			max_ref + reorder + 3;
		if (cap_count < 6)
			cap_count = 6;
		if (cap_count > cap_max)
			cap_count = cap_max;
		reorder_depth = reorder;
	}

	frame_dur = fps_override > 0 ? 1000000u / fps_override :
		    (m->frame_duration_us ? m->frame_duration_us : 33333);

	printf("h264: %ux%u max_ref=%u cap_bufs=%u frame_dur=%uus\n", width,
	       height, max_ref, cap_count, frame_dur);

	if (vdec_find_device(video_path, sizeof(video_path), media_path,
			     sizeof(media_path)) < 0)
		return -1;
	printf("device: %s + %s\n", video_path, media_path);

	if (vdec_open(&vdec, video_path, media_path, width, height, cap_count,
		      2, 512 * 1024) < 0)
		return -1;

	printf("capture: %ux%u bpl=%u sizeimage=%u uv_off=%u\n",
	       vdec.cap_width, vdec.cap_height, vdec.cap_bytesperline,
	       vdec.cap_sizeimage, vdec.cap_uv_offset);

	if (m->max_sample_size > 512 * 1024) {
		fprintf(stderr, "max sample %u exceeds output buffer\n",
			m->max_sample_size);
		goto out_vdec;
	}

	if (display_open(&disp, &vdec, plane_index) < 0)
		goto out_vdec;

	h264_dpb_init(&dpb, cap_count, max_ref, max_frame_num, reorder_depth);
	next_frame = now_us();

	for (i = 0; i < m->samples_count && (!limit || decode_count < limit);
	     i++) {
		const uint8_t *au = NULL;
		unsigned int cursor = 0, vcl_count = 0;
		struct nalu n, vcl = { 0 };
		struct h264_slice_hdr hdr;
		struct h264_poc poc;
		struct vdec_h264_ctrls ctrl;
		bool have_hdr = false;
		long long t0;
		int slot;
		uint64_t ts;

		if (mp4_read_sample(m, i, &au, NULL) != MP4_OK || !au)
			break;

		while (nalu_next_length_prefixed(au, m->samples[i].size,
						 m->nal_length_size, &cursor,
						 &n)) {
			unsigned int t = nalu_h264_type(&n);

			if (t == H264_NAL_SPS || t == H264_NAL_PPS) {
				h264_parser_parse_param_nal(&parser, &n);
			} else if (t == H264_NAL_SLICE || t == H264_NAL_IDR) {
				vcl_count++;
				if (have_hdr)
					continue;
				if (h264_parser_parse_slice(&parser, &n,
							    &hdr) < 0)
					break;
				h264_parser_compute_poc(&parser, &hdr, &poc);
				vcl = n;
				have_hdr = true;
			}
		}
		if (!have_hdr)
			continue;
		if (vcl_count > 1) {
			fprintf(stderr, "frame %u: %u slices, unsupported\n",
				i, vcl_count);
			rc = -1;
			goto out_display;
		}

		if (hdr.idr) {
			while ((out = h264_dpb_next_output(&dpb, true)) >= 0) {
				display_show(&disp, &vdec, out);
				h264_dpb_mark_displayed(&dpb, out);
				display_count++;
			}
		}

		if (h264_parser_fill_controls(&parser, &hdr, &poc, &ctrl) < 0)
			continue;

		slot = h264_dpb_begin_frame(&dpb, &hdr, &poc, &ts, &ctrl);
		if (slot < 0) {
			fprintf(stderr, "no free slot @%u\n", i);
			continue;
		}

		t0 = now_us();
		if (vdec_decode(&vdec, slot, ts, vcl.data, vcl.size,
				&ctrl) < 0) {
			fprintf(stderr, "decode failed frame %u (%s)\n", i,
				slice_type_str(hdr.slice_type));
			h264_dpb_abort_frame(&dpb);
			continue;
		}
		decode_us_total += now_us() - t0;
		decode_count++;

		if (dump_n && decode_count > dump_start &&
		    decode_count <= dump_start + dump_n)
			dump_frame(&vdec, slot, decode_count - 1);

		h264_dpb_end_frame(&dpb, &hdr);

		while ((out = h264_dpb_next_output(&dpb, false)) >= 0) {
			long long now = now_us();

			if (now < next_frame)
				usleep(next_frame - now);
			next_frame += frame_dur;
			if (display_show(&disp, &vdec, out) < 0)
				fprintf(stderr, "display failed\n");
			/*
			 * 在屏帧 + 上一帧都押住不许复用：SetPlane 返回不代表
			 * 离屏，DEFE 对旧 buffer 的引用还会撑一拍，立即复用
			 * 会被 VE 覆写出黑闪/前后跳(和 app 的 prev_item 同理)
			 */
			h264_dpb_set_on_screen(&dpb, out, true);
			if (held[1] >= 0)
				h264_dpb_set_on_screen(&dpb, held[1], false);
			held[1] = held[0];
			held[0] = out;
			h264_dpb_mark_displayed(&dpb, out);
			display_count++;
		}
	}

	while ((out = h264_dpb_next_output(&dpb, true)) >= 0) {
		usleep(frame_dur);
		display_show(&disp, &vdec, out);
		h264_dpb_set_on_screen(&dpb, out, true);
		if (held[1] >= 0)
			h264_dpb_set_on_screen(&dpb, held[1], false);
		held[1] = held[0];
		held[0] = out;
		h264_dpb_mark_displayed(&dpb, out);
		display_count++;
	}

	printf("decoded %u, displayed %u frames, avg decode %lld us/frame\n",
	       decode_count, display_count,
	       decode_count ? decode_us_total / decode_count : 0);
	rc = decode_count > 0 ? 0 : -1;

out_display:
	display_close(&disp, &vdec);
out_vdec:
	vdec_close(&vdec);
	return rc;
}

#endif /* !VDEC_SELFTEST_HOST_ONLY */

int main(int argc, char *argv[])
{
	struct mp4_demux m;
	const char *input = NULL;
	bool host_only = false;
	int fps = 0, plane_index = 0;
	unsigned int dump_n = 0, dump_start = 0, limit = 0;
	int opt, rc;

	while ((opt = getopt(argc, argv, "Hf:p:w:s:n:")) != -1) {
		switch (opt) {
		case 'H': host_only = true; break;
		case 'f': fps = atoi(optarg); break;
		case 'p': plane_index = atoi(optarg); break;
		case 'w': dump_n = (unsigned int)atoi(optarg); break;
		case 's': dump_start = (unsigned int)atoi(optarg); break;
		case 'n': limit = (unsigned int)atoi(optarg); break;
		default:
			fprintf(stderr,
				"usage: %s [-H] [-f fps] [-p plane] [-w n] [-s n] [-n n] file.mp4\n",
				argv[0]);
			return 1;
		}
	}
	if (optind >= argc) {
		fprintf(stderr, "missing input file\n");
		return 1;
	}
	input = argv[optind];

	if (mp4_open(&m, input) < 0)
		return 1;
	if (m.codec != MP4_CODEC_H264) {
		fprintf(stderr, "not an H264 mp4\n");
		mp4_close(&m);
		return 1;
	}

	rc = host_check(&m);

#ifndef VDEC_SELFTEST_HOST_ONLY
	if (!host_only && rc == 0)
		rc = device_decode(&m, fps, plane_index, dump_n, dump_start, limit);
#else
	(void)host_only; (void)fps; (void)plane_index; (void)dump_n;
	(void)dump_start; (void)limit;
#endif

	mp4_close(&m);
	return rc ? 1 : 0;
}
