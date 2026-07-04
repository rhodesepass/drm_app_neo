/*
 * NAL unit splitting. See nalu.h.
 */

#include "nalu.h"

bool nalu_next_length_prefixed(const uint8_t *au, unsigned int au_size,
			       unsigned int len_size, unsigned int *cursor,
			       struct nalu *out)
{
	unsigned int pos = *cursor;
	unsigned int nal_size = 0;
	unsigned int i;

	if (len_size < 1 || len_size > 4)
		return false;
	if (pos + len_size > au_size)
		return false;

	for (i = 0; i < len_size; i++)
		nal_size = (nal_size << 8) | au[pos + i];
	pos += len_size;

	if (nal_size == 0 || pos + nal_size > au_size)
		return false;

	out->data = au + pos;
	out->size = nal_size;
	*cursor = pos + nal_size;
	return true;
}

/* Return offset of next start code at or after `from`, or `size` if none.
 * `*sc_len` is set to the start code length (3 or 4). */
static unsigned int find_start_code(const uint8_t *buf, unsigned int size,
				    unsigned int from, unsigned int *sc_len)
{
	unsigned int i;

	for (i = from; i + 3 <= size; i++) {
		if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) {
			if (i >= 1 && buf[i - 1] == 0)
				*sc_len = 4;
			else
				*sc_len = 3;
			return i;
		}
	}
	return size;
}

bool nalu_next_annexb(const uint8_t *buf, unsigned int size,
		      unsigned int *cursor, struct nalu *out)
{
	unsigned int pos = *cursor;
	unsigned int sc_len, start, nal_start, next, next_sc_len;

	if (pos >= size)
		return false;

	start = find_start_code(buf, size, pos, &sc_len);
	if (start >= size)
		return false;

	nal_start = start + sc_len;
	if (nal_start >= size)
		return false;

	next = find_start_code(buf, size, nal_start, &next_sc_len);
	/* A 4-byte start code's leading zero belongs to the trailing of the
	 * previous NAL; trim it off. */
	{
		unsigned int nal_end = next;
		if (next < size && next_sc_len == 4)
			nal_end = next - 1;

		out->data = buf + nal_start;
		out->size = nal_end - nal_start;
	}

	*cursor = next;
	return out->size > 0;
}
