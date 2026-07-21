/*
 * Minimal, zero-dependency ISO-BMFF (MP4) demuxer.
 *
 * Extracts just what a stateless video decoder needs from the first video
 * track: codec, NAL length prefix size, decoder config record (avcC/hvcC),
 * coded dimensions and the list of samples (access units) in decode order.
 *
 * The header (moov) is mmap'd only during mp4_open() to parse metadata; the
 * mapping is dropped before returning. Sample data is read on demand via
 * pread() into a reusable buffer, so a yanked SD card surfaces as an EIO
 * return (MP4_ERR_IO) instead of a SIGBUS on a faulting mmap page.
 */

#ifndef _VDEC_MP4_DEMUX_H_
#define _VDEC_MP4_DEMUX_H_

#include <stdbool.h>
#include <stdint.h>

enum mp4_codec {
	MP4_CODEC_UNKNOWN = 0,
	MP4_CODEC_H264,
	MP4_CODEC_H265,
};

struct mp4_sample {
	uint64_t offset;	/* byte offset in file */
	uint32_t size;
	bool sync;		/* sync (key) sample */
};

enum {
	MP4_OK        =  0,
	MP4_ERR_RANGE = -1,	/* index/table out of range */
	MP4_ERR_IO    = -2,	/* pread failed/short — source likely gone */
};

struct mp4_demux {
	int fd;
	const uint8_t *map;	/* transient: whole-file mmap, only alive during mp4_open() */
	uint64_t map_size;

	enum mp4_codec codec;
	unsigned int width;
	unsigned int height;
	unsigned int nal_length_size;	/* 1..4 */

	uint8_t *extradata;	/* avcC/hvcC payload — owned copy lifted out of the map */
	unsigned int extradata_size;

	struct mp4_sample *samples;
	unsigned int samples_count;

	/* reusable per-sample read buffer (sized to max_sample_size) */
	uint8_t *sample_buf;
	uint32_t sample_buf_cap;

	/* mdhd timescale + first stts delta; 0 if absent (caller falls back) */
	uint32_t frame_duration_us;
	uint32_t max_sample_size;
	uint32_t stts_delta;	/* internal (media timescale units) */
};

int mp4_open(struct mp4_demux *m, const char *path);
void mp4_close(struct mp4_demux *m);

/* Read sample `index` into the demux's reusable buffer via pread().
 * On MP4_OK, *data points to the buffer (valid until the next call) and *size
 * is the sample size. MP4_ERR_RANGE: bad index. MP4_ERR_IO: read failed/short
 * (SD card likely removed) — caller should stop playback. */
int mp4_read_sample(struct mp4_demux *m, unsigned int index,
		    const uint8_t **data, uint32_t *size);

#endif /* _VDEC_MP4_DEMUX_H_ */
