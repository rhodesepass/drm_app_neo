/*
 * Emulation-prevention-aware bitstream reader for H.264/H.265 RBSP parsing.
 *
 * Semantics mirror GStreamer's NalReader (gst-plugins-bad nalutils.c, LGPL):
 *  - read transparently skips emulation_prevention_three_byte (0x03 after 00 00)
 *  - br_pos() returns the *raw* bit position (counts skipped EPB bytes as 8 bits),
 *    so it matches gst's nal_reader_get_pos() used to compute header_bit_size
 *  - br_epb_count() returns the number of EPB bytes skipped so far
 *
 * This exact correspondence is required to fill V4L2 stateless controls
 * (H264 slice_params.header_bit_size, HEVC slice_params.data_byte_offset).
 */

#ifndef _VDEC_BITREADER_H_
#define _VDEC_BITREADER_H_

#include <stdbool.h>
#include <stdint.h>

struct bitreader {
	const uint8_t *data;
	unsigned int size;	/* bytes */
	unsigned int n_epb;	/* emulation prevention bytes skipped */

	unsigned int byte;	/* next byte index to consume (incl. EPB) */
	unsigned int bits_in_cache;
	uint8_t first_byte;
	uint32_t epb_cache;	/* last 3 bytes, to detect EPB */
	uint64_t cache;
};

void br_init(struct bitreader *br, const uint8_t *data, unsigned int size);

/* Read up to 32 bits; returns false on out-of-data (value untouched). */
bool br_read(struct bitreader *br, uint32_t *val, unsigned int nbits);
/* Read 1 bit; returns the bit value (0 on error). */
uint32_t br_u1(struct bitreader *br);
/* Read nbits (<=32); returns value (0 on error). Convenience wrapper. */
uint32_t br_u(struct bitreader *br, unsigned int nbits);

bool br_skip(struct bitreader *br, unsigned int nbits);

/* Exp-Golomb. Return false on error. */
bool br_ue(struct bitreader *br, uint32_t *val);
bool br_se(struct bitreader *br, int32_t *val);
/* Convenience: return value directly (0 on error). */
uint32_t br_ue_v(struct bitreader *br);
int32_t br_se_v(struct bitreader *br);

unsigned int br_pos(const struct bitreader *br);		/* raw bits consumed */
unsigned int br_remaining(const struct bitreader *br);		/* bits left */
unsigned int br_epb_count(const struct bitreader *br);
bool br_byte_aligned(const struct bitreader *br);
bool br_more_rbsp_data(const struct bitreader *br);

#endif /* _VDEC_BITREADER_H_ */
