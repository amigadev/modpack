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

#define TP61A_CHANNEL_BYTES (3)

#define TP61A_UC_FINETUNE               (0x00000001) // Finetune
#define TP61A_UC_PORTAMENTO_UP          (0x00000002) // Portamento Down
#define TP61A_UC_PORTAMENTO_DOWN        (0x00000004) // Portamento Up
#define TP61A_UC_TONE_PORTAMENTO        (0x00000028) // Tone Portamento 40? = 0x28 0x20 0x08
#define TP61A_UC_VIBRATO                (0x00000050) // Vibrato 80? = 0x50 0x40 0x10
#define TP61A_UC_TP_VS                  (0x00000020) // Tone Portamento + Volume Slide
#define TP61A_UC_VB_VS                  (0x00000040) // Vibrato + Volume Slide
#define TP61A_UC_TREMOLO                (0x00000080) // Tremolo
#define TP61A_UC_ARPEGGIO               (0x00000100) // Arpeggio
#define TP61A_UC_SAMPLE_OFFSET          (0x00000200) // Sample Offset
#define TP61A_UC_VOLUME_SLIDE           (0x00000400) // Volume Slide
#define TP61A_UC_POSITION_JUMP          (0x00000800) // Position Jump
#define TP61A_UC_SET_VOLUME             (0x00001000) // Set Volume
#define TP61A_UC_PATTERN_BREAK          (0x00002800) // Pattern Break
#define TP61A_UC_SET_SPEED              (0x00008000) // Set Speed
#define TP61A_UC_EXTENDED               (0xffff0000) // Extended Commands
#define TP61A_UC_E_FILTER               (0x00010000) // Extended: Filter
#define TP61A_UC_E_FINE_SLIDE_UP        (0x00020000) // Extended: Fine Slide Up
#define TP61A_UC_E_FINE_SLIDE_DOWN      (0x00040000) // Extended: Fine Slide Down
#define TP61A_UC_E_SET_FINETUNE         (0x00200000) // Extended: Set Finetune
#define TP61A_UC_E_PATTERN_LOOP         (0x00400000) // Extended: Pattern Loop
#define TP61A_UC_E_TIMING               (0x01000000) // Extended: Timing (E8)
#define TP61A_UC_E_RETRIGGER_NOTE       (0x02000000) // Extended: Re-trigger Note
#define TP61A_UC_E_FINE_VOLUME_S_UP     (0x04000000) // Extended: Fine Volume Slide Up
#define TP61A_UC_E_FINE_VOLUME_S_DOWN   (0x08000000) // Extended: Fine Volume Slide Down
#define TP61A_UC_E_NOTE_CUT             (0x10000000) // Extended: Note Cut
#define TP61A_UC_E_NOTE_DELAY           (0x20000000) // Extended: Note Delay
#define TP61A_UC_E_PATTERN_DELAY        (0x40000000) // Extended: Pattern Delay
#define TP61A_UC_E_INVERT_LOOP          (0x80000000) // Extended: Invert Loop

typedef struct __attribute__((__packed__))
{
    uint16_t length;            // length in words (when looping, subtract repeat offset for loop length)
    uint8_t finetone;           // 0x0f = finetone, 0x20 = sample compression
    uint8_t volume;             // 0-64
    uint16_t repeat_offset;     // offset in words, 0xffff = no loop
} player61a_sample_t;

typedef struct __attribute__((__packed__))
{
    uint16_t sample_offset;
    uint8_t num_patterns;
    uint8_t sample_count; // 0x1f = sample count (1-31), 0x20 = 4-bit compression 0x40 = delta compression
} player61a_header_t;

typedef struct __attribute__((__packed__))
{
    uint16_t channels[PT_NUM_CHANNELS];
} player61a_pattern_offset_t;

typedef struct
{
    size_t length;
    uint8_t positions[PT_NUM_POSITIONS];
} player61a_song_t;

typedef struct
{
    uint8_t data[TP61A_CHANNEL_BYTES];
} player61a_channel_t;

typedef struct
{
    uint32_t usecode;

    // samples

    player61a_header_t header;
    player61a_sample_t sample_headers[PT_NUM_SAMPLES];

    player61a_song_t song;

    player61a_pattern_offset_t* pattern_offsets;

    buffer_t patterns;
    buffer_t samples;
} player61a_t;

bool player61a_convert(buffer_t* buffer, const protracker_t* module, const char* opts);
