#pragma once

#include "protracker.h"

/*
	swap	d4
	move.l	usec(a5),d2
	cmp	#$e,d4
	bne.b	eie
	move	#$f,d4
	rol	#4,d1
	and	d1,d4
	add	#16,d4				; E-command -> Add 16
eie	bset	d4,d2
	move.l	d2,usec(a5)			; Mark command used
*/

#define P61A_CHANNEL_BYTES (3)

typedef struct __attribute__((__packed__))
{
    uint16_t length;            // length in words (when looping, subtract repeat offset for loop length)
    uint8_t finetone;           // 0x0f = finetone, 0x80 = 4-bit packed sample data
    uint8_t volume;             // 0-64
    uint16_t repeat_offset;     // offset in words, 0xffff = no loop
} p61a_sample_t;

typedef struct __attribute__((__packed__))
{
    uint16_t sample_offset;
    uint8_t pattern_count;
    uint8_t sample_count; // 0x1f = sample count (0-31), 0x40 = 4-bit packed samples (buffer size follows), 0x80 = 8-bit delta samples
} p61a_header_t;

typedef struct __attribute__((__packed__))
{
    uint16_t channels[PT_NUM_CHANNELS];
} p61a_pattern_offset_t;

typedef struct
{
    size_t length;
    uint8_t positions[PT_NUM_POSITIONS];
} p61a_song_t;

typedef struct
{
    uint8_t data[P61A_CHANNEL_BYTES];
} p61a_channel_t;

typedef struct
{
    p61a_channel_t channels[PT_NUM_CHANNELS];
} p61a_pattern_row_t;

typedef struct
{
    p61a_pattern_row_t rows[PT_PATTERN_ROWS];
} p61a_pattern_t;

typedef struct
{
    uint32_t usecode;

    // samples

    p61a_header_t header;
    uint32_t sample_buffer_size;
    p61a_sample_t sample_headers[PT_NUM_SAMPLES];
    uint8_t sample_map[PT_NUM_SAMPLES];

    p61a_song_t song;

    p61a_pattern_offset_t* pattern_offsets;

    buffer_t patterns;
    buffer_t samples;
} player61a_t;

bool player61a_convert(buffer_t* buffer, const protracker_t* module, const char* opts);
protracker_t* player61a_load(const buffer_t* buffer);

