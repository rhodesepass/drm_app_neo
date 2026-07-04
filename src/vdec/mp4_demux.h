/*
 * Minimal, zero-dependency ISO-BMFF (MP4) demuxer.
 *
 * Extracts just what a stateless video decoder needs from the first video
 * track: codec, NAL length prefix size, decoder config record (avcC/hvcC),
 * coded dimensions and the list of samples (access units) in decode order.
 *
 * The file is mmap'd read-only; sample data is referenced in place.
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

struct mp4_demux {
	int fd;
	const uint8_t *map;	/* whole-file mmap */
	uint64_t map_size;

	enum mp4_codec codec;
	unsigned int width;
	unsigned int height;
	unsigned int nal_length_size;	/* 1..4 */

	const uint8_t *extradata;	/* avcC/hvcC payload (in map) */
	unsigned int extradata_size;

	struct mp4_sample *samples;
	unsigned int samples_count;

	/* mdhd timescale + first stts delta; 0 if absent (caller falls back) */
	uint32_t frame_duration_us;
	uint32_t max_sample_size;
	uint32_t stts_delta;	/* internal (media timescale units) */
};

int mp4_open(struct mp4_demux *m, const char *path);
void mp4_close(struct mp4_demux *m);

/* Pointer to sample data within the mmap (no copy). NULL if out of range. */
const uint8_t *mp4_sample_data(const struct mp4_demux *m, unsigned int index);

#endif /* _VDEC_MP4_DEMUX_H_ */
