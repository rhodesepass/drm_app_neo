/*
 * NAL unit splitting for H.264/H.265, both Annex-B (start codes) and
 * length-prefixed (MP4 avcC/hvcC) forms.
 *
 * A `struct nalu` points at the NAL unit starting from its header byte
 * (no start code, no length prefix).
 */

#ifndef _VDEC_NALU_H_
#define _VDEC_NALU_H_

#include <stdbool.h>
#include <stdint.h>

struct nalu {
	const uint8_t *data;	/* points at NAL header byte */
	unsigned int size;	/* bytes, including NAL header */
};

/* H.264: type = data[0] & 0x1f (header_bytes = 1). */
static inline unsigned int nalu_h264_type(const struct nalu *n)
{
	return n->size ? (n->data[0] & 0x1f) : 0;
}

/* H.265: type = (data[0] >> 1) & 0x3f (header_bytes = 2). */
static inline unsigned int nalu_h265_type(const struct nalu *n)
{
	return n->size ? ((n->data[0] >> 1) & 0x3f) : 0;
}

/*
 * Iterate length-prefixed NAL units (e.g. one MP4 sample / access unit).
 * `len_size` is lengthSizeMinusOne+1 (1..4). `*cursor` starts at 0.
 * Returns true and fills `out` for each NAL; false when exhausted/malformed.
 */
bool nalu_next_length_prefixed(const uint8_t *au, unsigned int au_size,
			       unsigned int len_size, unsigned int *cursor,
			       struct nalu *out);

/*
 * Iterate Annex-B NAL units (start-code separated). `*cursor` starts at 0.
 * Returns true and fills `out` for each NAL; false when exhausted.
 */
bool nalu_next_annexb(const uint8_t *buf, unsigned int size,
		      unsigned int *cursor, struct nalu *out);

#endif /* _VDEC_NALU_H_ */
