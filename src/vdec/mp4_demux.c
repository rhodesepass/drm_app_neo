/*
 * Minimal ISO-BMFF (MP4) demuxer. See mp4_demux.h.
 */

#include "mp4_demux.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FOURCC(a, b, c, d) \
	(((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
	 ((uint32_t)(c) << 8) | (uint32_t)(d))

static uint32_t rd_u16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static uint32_t rd_u32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static uint64_t rd_u64(const uint8_t *p)
{
	return ((uint64_t)rd_u32(p) << 32) | rd_u32(p + 4);
}

/* Box iterator over a buffer [data, data+size). */
struct box_iter {
	const uint8_t *p;
	const uint8_t *end;
};

static void box_iter_init(struct box_iter *it, const uint8_t *data, uint64_t size)
{
	it->p = data;
	it->end = data + size;
}

static bool box_next(struct box_iter *it, uint32_t *type,
		     const uint8_t **payload, uint64_t *payload_size)
{
	const uint8_t *p = it->p;
	uint64_t size;
	unsigned int hdr = 8;

	if (p + 8 > it->end)
		return false;

	size = rd_u32(p);
	*type = rd_u32(p + 4);

	if (size == 1) {
		if (p + 16 > it->end)
			return false;
		size = rd_u64(p + 8);
		hdr = 16;
	} else if (size == 0) {
		size = (uint64_t)(it->end - p);
	}

	if (size < hdr || p + size > it->end)
		return false;

	*payload = p + hdr;
	*payload_size = size - hdr;
	it->p = p + size;
	return true;
}

/* Find first child box of `type` within [data, size). */
static bool find_box(const uint8_t *data, uint64_t size, uint32_t type,
		     const uint8_t **payload, uint64_t *payload_size)
{
	struct box_iter it;
	uint32_t t;

	box_iter_init(&it, data, size);
	while (box_next(&it, &t, payload, payload_size))
		if (t == type)
			return true;
	return false;
}

/* Parse stsd to detect codec, extradata, and NAL length size. */
static int parse_stsd(struct mp4_demux *m, const uint8_t *data, uint64_t size)
{
	const uint8_t *entry, *cfg;
	uint64_t cfg_size;
	uint32_t fmt;
	uint32_t entry_count;

	if (size < 8)
		return -1;
	entry_count = rd_u32(data + 4);
	if (entry_count == 0)
		return -1;

	/* First sample entry box starts at offset 8. */
	entry = data + 8;
	if (entry + 8 > data + size)
		return -1;

	fmt = rd_u32(entry + 4);
	switch (fmt) {
	case FOURCC('a', 'v', 'c', '1'):
	case FOURCC('a', 'v', 'c', '3'):
		m->codec = MP4_CODEC_H264;
		break;
	case FOURCC('h', 'v', 'c', '1'):
	case FOURCC('h', 'e', 'v', '1'):
		m->codec = MP4_CODEC_H265;
		break;
	default:
		fprintf(stderr, "mp4: unsupported sample format '%.4s'\n",
			(const char *)(entry + 4));
		return -1;
	}

	/* VisualSampleEntry: width/height at offset 8(base)+16 / +18 within
	 * entry content (after the 8-byte box header). */
	if (entry + 8 + 28 <= data + size) {
		m->width = rd_u16(entry + 8 + 24);
		m->height = rd_u16(entry + 8 + 26);
	}

	/* Child config box (avcC/hvcC) starts after base(8)+visual(70)=78. */
	{
		uint64_t entry_size = rd_u32(entry);
		const uint8_t *children = entry + 8 + 78;
		uint64_t children_size;

		if (entry_size < 8 + 78 || entry + entry_size > data + size)
			return -1;
		children_size = entry_size - (8 + 78);

		uint32_t cfg_type = (m->codec == MP4_CODEC_H264) ?
			FOURCC('a', 'v', 'c', 'C') : FOURCC('h', 'v', 'c', 'C');
		if (!find_box(children, children_size, cfg_type, &cfg, &cfg_size)) {
			fprintf(stderr, "mp4: missing decoder config box\n");
			return -1;
		}
	}

	m->extradata = cfg;
	m->extradata_size = (unsigned int)cfg_size;

	if (m->codec == MP4_CODEC_H264) {
		if (cfg_size < 5)
			return -1;
		m->nal_length_size = (cfg[4] & 0x3) + 1;
	} else {
		if (cfg_size < 23)
			return -1;
		m->nal_length_size = (cfg[21] & 0x3) + 1;
	}

	return 0;
}

/* Build the sample list from stsz/stsc/stco(co64). */
static int build_samples(struct mp4_demux *m, const uint8_t *stbl,
			 uint64_t stbl_size)
{
	const uint8_t *stsz, *stsc, *stco, *co64, *stts, *stss;
	uint64_t stsz_sz, stsc_sz, stco_sz, co64_sz, stts_sz, stss_sz;
	uint32_t sample_count, sample_size_default;
	uint32_t chunk_count;
	bool is_co64 = false;
	unsigned int i, s;

	if (!find_box(stbl, stbl_size, FOURCC('s', 't', 's', 'z'), &stsz, &stsz_sz)) {
		fprintf(stderr, "mp4: missing stsz\n");
		return -1;
	}
	if (!find_box(stbl, stbl_size, FOURCC('s', 't', 's', 'c'), &stsc, &stsc_sz)) {
		fprintf(stderr, "mp4: missing stsc\n");
		return -1;
	}
	if (find_box(stbl, stbl_size, FOURCC('s', 't', 'c', 'o'), &stco, &stco_sz)) {
		is_co64 = false;
	} else if (find_box(stbl, stbl_size, FOURCC('c', 'o', '6', '4'), &co64,
			    &co64_sz)) {
		is_co64 = true;
		stco = co64;
		stco_sz = co64_sz;
	} else {
		fprintf(stderr, "mp4: missing stco/co64\n");
		return -1;
	}

	if (stsz_sz < 12)
		return -1;
	sample_size_default = rd_u32(stsz + 4);
	sample_count = rd_u32(stsz + 8);
	if (sample_count == 0)
		return -1;

	chunk_count = (stco_sz >= 8) ? rd_u32(stco + 4) : 0;
	if (chunk_count == 0)
		return -1;

	m->samples = calloc(sample_count, sizeof(*m->samples));
	if (!m->samples)
		return -1;
	m->samples_count = sample_count;

	/* Per-sample sizes. */
	for (s = 0; s < sample_count; s++) {
		if (sample_size_default != 0) {
			m->samples[s].size = sample_size_default;
		} else {
			const uint8_t *e = stsz + 12 + (uint64_t)s * 4;
			if (e + 4 > stsz + stsz_sz)
				goto fail;
			m->samples[s].size = rd_u32(e);
		}
		m->samples[s].sync = true; /* default: all sync unless stss */
		if (m->samples[s].size > m->max_sample_size)
			m->max_sample_size = m->samples[s].size;
	}

	/* Walk stsc to know samples-per-chunk, then assign offsets. */
	{
		uint32_t stsc_entries = (stsc_sz >= 8) ? rd_u32(stsc + 4) : 0;
		const uint8_t *e = stsc + 8;
		uint32_t sample_idx = 0;
		uint32_t ci;

		if (stsc_entries == 0)
			goto fail;

		for (ci = 0; ci < chunk_count && sample_idx < sample_count; ci++) {
			uint32_t chunk_no = ci + 1; /* 1-based */
			uint32_t spc = 0;
			uint32_t k;
			uint64_t off;

			/* find samples-per-chunk for this chunk: last entry whose
			 * first_chunk <= chunk_no */
			for (k = 0; k < stsc_entries; k++) {
				const uint8_t *ek = e + (uint64_t)k * 12;
				uint32_t first_chunk;
				if (ek + 12 > stsc + stsc_sz)
					goto fail;
				first_chunk = rd_u32(ek);
				if (first_chunk <= chunk_no)
					spc = rd_u32(ek + 4);
				else
					break;
			}
			if (spc == 0)
				continue;

			/* chunk offset */
			if (is_co64)
				off = rd_u64(stco + 8 + (uint64_t)ci * 8);
			else
				off = rd_u32(stco + 8 + (uint64_t)ci * 4);

			for (k = 0; k < spc && sample_idx < sample_count; k++) {
				m->samples[sample_idx].offset = off;
				off += m->samples[sample_idx].size;
				sample_idx++;
			}
		}
		if (sample_idx < sample_count) {
			fprintf(stderr,
				"mp4: only mapped %u/%u samples\n",
				sample_idx, sample_count);
			m->samples_count = sample_idx;
		}
	}

	/* Sync samples (stss). If present, only listed samples are sync. */
	if (find_box(stbl, stbl_size, FOURCC('s', 't', 's', 's'), &stss, &stss_sz)) {
		uint32_t n = (stss_sz >= 8) ? rd_u32(stss + 4) : 0;
		for (s = 0; s < m->samples_count; s++)
			m->samples[s].sync = false;
		for (i = 0; i < n; i++) {
			const uint8_t *e = stss + 8 + (uint64_t)i * 4;
			uint32_t idx;
			if (e + 4 > stss + stss_sz)
				break;
			idx = rd_u32(e);
			if (idx >= 1 && idx <= m->samples_count)
				m->samples[idx - 1].sync = true;
		}
	}

	/* stts: use the first entry's delta (assets are constant frame rate) */
	if (find_box(stbl, stbl_size, FOURCC('s', 't', 't', 's'), &stts, &stts_sz) &&
	    stts_sz >= 16 && rd_u32(stts + 4) >= 1)
		m->stts_delta = rd_u32(stts + 12);

	return 0;

fail:
	free(m->samples);
	m->samples = NULL;
	m->samples_count = 0;
	return -1;
}

/* Returns true if the trak is the video track and gets parsed into m. */
static bool parse_trak(struct mp4_demux *m, const uint8_t *trak, uint64_t size)
{
	const uint8_t *mdia, *hdlr, *minf, *stbl, *stsd, *tkhd;
	uint64_t mdia_sz, hdlr_sz, minf_sz, stbl_sz, stsd_sz, tkhd_sz;

	if (!find_box(trak, size, FOURCC('m', 'd', 'i', 'a'), &mdia, &mdia_sz))
		return false;
	if (!find_box(mdia, mdia_sz, FOURCC('h', 'd', 'l', 'r'), &hdlr, &hdlr_sz))
		return false;
	/* hdlr: version/flags(4) pre_defined(4) handler_type(4) */
	if (hdlr_sz < 12 || rd_u32(hdlr + 8) != FOURCC('v', 'i', 'd', 'e'))
		return false;

	if (!find_box(mdia, mdia_sz, FOURCC('m', 'i', 'n', 'f'), &minf, &minf_sz))
		return false;
	if (!find_box(minf, minf_sz, FOURCC('s', 't', 'b', 'l'), &stbl, &stbl_sz))
		return false;
	if (!find_box(stbl, stbl_sz, FOURCC('s', 't', 's', 'd'), &stsd, &stsd_sz))
		return false;

	if (parse_stsd(m, stsd, stsd_sz) < 0)
		return false;

	/* tkhd dimensions (16.16 fixed) override SampleEntry if present. */
	if (find_box(trak, size, FOURCC('t', 'k', 'h', 'd'), &tkhd, &tkhd_sz) &&
	    tkhd_sz >= 8) {
		unsigned int w = rd_u32(tkhd + tkhd_sz - 8) >> 16;
		unsigned int h = rd_u32(tkhd + tkhd_sz - 4) >> 16;
		if (w && h) {
			m->width = w;
			m->height = h;
		}
	}

	if (build_samples(m, stbl, stbl_sz) < 0)
		return false;

	/* mdhd: timescale at +12 (version 0) / +20 (version 1) */
	{
		const uint8_t *mdhd;
		uint64_t mdhd_sz;
		uint32_t timescale = 0;

		if (find_box(mdia, mdia_sz, FOURCC('m', 'd', 'h', 'd'), &mdhd,
			     &mdhd_sz) && mdhd_sz >= 16)
			timescale = rd_u32(mdhd + (mdhd[0] == 1 ? 20 : 12));

		if (timescale && m->stts_delta)
			m->frame_duration_us = (uint32_t)((uint64_t)m->stts_delta *
							  1000000 / timescale);
	}

	return true;
}

int mp4_open(struct mp4_demux *m, const char *path)
{
	struct stat st;
	const uint8_t *moov;
	uint64_t moov_sz;
	struct box_iter it;
	uint32_t type;
	const uint8_t *payload;
	uint64_t payload_sz;
	bool found = false;

	memset(m, 0, sizeof(*m));
	m->fd = -1;

	m->fd = open(path, O_RDONLY);
	if (m->fd < 0) {
		fprintf(stderr, "mp4: cannot open %s\n", path);
		return -1;
	}
	if (fstat(m->fd, &st) < 0 || st.st_size <= 0)
		goto fail;

	m->map_size = (uint64_t)st.st_size;
	m->map = mmap(NULL, m->map_size, PROT_READ, MAP_PRIVATE, m->fd, 0);
	if (m->map == MAP_FAILED) {
		m->map = NULL;
		fprintf(stderr, "mp4: mmap failed\n");
		goto fail;
	}

	if (!find_box(m->map, m->map_size, FOURCC('m', 'o', 'o', 'v'), &moov,
		      &moov_sz)) {
		fprintf(stderr, "mp4: no moov box\n");
		goto fail;
	}

	/* Iterate trak boxes, pick the video one. */
	box_iter_init(&it, moov, moov_sz);
	while (box_next(&it, &type, &payload, &payload_sz)) {
		if (type != FOURCC('t', 'r', 'a', 'k'))
			continue;
		if (parse_trak(m, payload, payload_sz)) {
			found = true;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "mp4: no usable video track\n");
		goto fail;
	}

	return 0;

fail:
	mp4_close(m);
	return -1;
}

void mp4_close(struct mp4_demux *m)
{
	if (m->samples)
		free(m->samples);
	if (m->map)
		munmap((void *)m->map, m->map_size);
	if (m->fd >= 0)
		close(m->fd);
	memset(m, 0, sizeof(*m));
	m->fd = -1;
}

const uint8_t *mp4_sample_data(const struct mp4_demux *m, unsigned int index)
{
	const struct mp4_sample *s;

	if (index >= m->samples_count)
		return NULL;
	s = &m->samples[index];
	if (s->offset + s->size > m->map_size)
		return NULL;
	return m->map + s->offset;
}
