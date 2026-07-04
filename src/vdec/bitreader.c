/*
 * Emulation-prevention-aware bitstream reader.
 * Semantics mirror GStreamer's NalReader (LGPL). See bitreader.h.
 */

#include "bitreader.h"

void br_init(struct bitreader *br, const uint8_t *data, unsigned int size)
{
	br->data = data;
	br->size = size;
	br->n_epb = 0;
	br->byte = 0;
	br->bits_in_cache = 0;
	br->first_byte = 0xff;
	br->epb_cache = 0xff;
	br->cache = 0xff;
}

bool br_read(struct bitreader *br, uint32_t *val, unsigned int nbits)
{
	unsigned int shift;

	if (nbits == 0) {
		*val = 0;
		return true;
	}
	if (nbits > 32)
		return false;

	if (br->byte * 8 + (nbits - br->bits_in_cache) > br->size * 8)
		return false;

	while (br->bits_in_cache < nbits) {
		uint8_t byte;

	next_byte:
		if (br->byte >= br->size)
			return false;

		byte = br->data[br->byte++];
		br->epb_cache = (br->epb_cache << 8) | byte;

		/* emulation_prevention_three_byte: 0x000003 */
		if ((br->epb_cache & 0xffffff) == 0x3) {
			br->n_epb++;
			goto next_byte;
		}

		br->cache = (br->cache << 8) | br->first_byte;
		br->first_byte = byte;
		br->bits_in_cache += 8;
	}

	shift = br->bits_in_cache - nbits;
	*val = br->first_byte >> shift;
	*val |= (uint32_t)(br->cache << (8 - shift));
	if (nbits < 32)
		*val &= ((uint32_t)1 << nbits) - 1;
	br->bits_in_cache = shift;

	return true;
}

uint32_t br_u(struct bitreader *br, unsigned int nbits)
{
	uint32_t v = 0;
	br_read(br, &v, nbits);
	return v;
}

uint32_t br_u1(struct bitreader *br)
{
	return br_u(br, 1);
}

bool br_skip(struct bitreader *br, unsigned int nbits)
{
	uint32_t dummy;

	while (nbits > 32) {
		if (!br_read(br, &dummy, 32))
			return false;
		nbits -= 32;
	}
	return br_read(br, &dummy, nbits);
}

bool br_ue(struct bitreader *br, uint32_t *val)
{
	unsigned int i = 0;
	uint32_t bit, value;

	if (!br_read(br, &bit, 1))
		return false;
	while (bit == 0) {
		i++;
		if (!br_read(br, &bit, 1))
			return false;
	}
	if (i > 31)
		return false;
	if (!br_read(br, &value, i))
		return false;

	*val = ((uint32_t)1 << i) - 1 + value;
	return true;
}

bool br_se(struct bitreader *br, int32_t *val)
{
	uint32_t value;

	if (!br_ue(br, &value))
		return false;

	if (value & 1)
		*val = (int32_t)((value / 2) + 1);
	else
		*val = -(int32_t)(value / 2);
	return true;
}

uint32_t br_ue_v(struct bitreader *br)
{
	uint32_t v = 0;
	br_ue(br, &v);
	return v;
}

int32_t br_se_v(struct bitreader *br)
{
	int32_t v = 0;
	br_se(br, &v);
	return v;
}

unsigned int br_pos(const struct bitreader *br)
{
	return br->byte * 8 - br->bits_in_cache;
}

unsigned int br_remaining(const struct bitreader *br)
{
	return (br->size - br->byte) * 8 + br->bits_in_cache;
}

unsigned int br_epb_count(const struct bitreader *br)
{
	return br->n_epb;
}

bool br_byte_aligned(const struct bitreader *br)
{
	return br->bits_in_cache == 0;
}

bool br_more_rbsp_data(const struct bitreader *br)
{
	struct bitreader tmp = *br;
	unsigned int remaining, nbits;
	uint32_t stop_bit, zero_bits;

	remaining = br_remaining(&tmp);
	if (remaining == 0)
		return false;

	if (!br_read(&tmp, &stop_bit, 1))
		return false;
	if (!stop_bit)
		return true;

	nbits = --remaining % 8;
	while (remaining > 0) {
		if (!br_read(&tmp, &zero_bits, nbits))
			return false;
		if (zero_bits != 0)
			return true;
		remaining -= nbits;
		nbits = 8;
	}
	return false;
}
