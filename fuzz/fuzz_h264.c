/*
 * H.264 参数集/切片头解析 fuzz target。直接喂 buffer，不经 MP4 容器。
 *
 * 输入约定: 第 1 字节 len_size(1..4, 决定按 length-prefixed 还是 annex-b 切分),
 * 其余为若干 NAL。这样一个语料同时覆盖 nalu 切分 + avcC/SPS/PPS/slice 解析。
 * SPS/PPS 走 param_nal 累积到 parser 状态, slice 复用已解析的参数集 ——
 * 正是 mediaplayer.c 的真实调用序列。
 */
#include "vdec/h264_parser.h"
#include "vdec/nalu.h"
#include "vdec/vdec.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 2)
		return 0;

	unsigned int len_size = (data[0] & 0x3) + 1;	/* 1..4 */
	const uint8_t *au = data + 1;
	unsigned int au_size = (unsigned int)(size - 1);

	struct h264_parser p;
	h264_parser_init(&p);

	/* 前 8 字节当 avcC extradata 喂一遍(容错解析) */
	h264_parser_parse_avcc(&p, au, au_size < 64 ? au_size : 64);

	struct nalu n;
	unsigned int cursor = 0;
	while (nalu_next_length_prefixed(au, au_size, len_size, &cursor, &n)) {
		unsigned int t = nalu_h264_type(&n);
		if (t == H264_NAL_SPS || t == H264_NAL_PPS) {
			h264_parser_parse_param_nal(&p, &n);
		} else if (t == H264_NAL_SLICE || t == H264_NAL_IDR) {
			struct h264_slice_hdr hdr;
			memset(&hdr, 0, sizeof(hdr));
			if (h264_parser_parse_slice(&p, &n, &hdr) == 0) {
				struct h264_poc poc;
				memset(&poc, 0, sizeof(poc));
				h264_parser_compute_poc(&p, &hdr, &poc);
				struct vdec_h264_ctrls ctrl;
				memset(&ctrl, 0, sizeof(ctrl));
				h264_parser_fill_controls(&p, &hdr, &poc, &ctrl);
			}
		}
	}

	/* 再用 annex-b 切分器过一遍同一 buffer，覆盖另一条路径 */
	cursor = 0;
	while (nalu_next_annexb(au, au_size, &cursor, &n)) {
		struct h264_slice_hdr hdr;
		memset(&hdr, 0, sizeof(hdr));
		unsigned int t = nalu_h264_type(&n);
		if (t == H264_NAL_SPS || t == H264_NAL_PPS)
			h264_parser_parse_param_nal(&p, &n);
	}

	return 0;
}
