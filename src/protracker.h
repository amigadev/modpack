#pragma once

#include <stdint.h>

#define PROTRACKER_MAX_SAMPLES  (32)
#define PROTRACKER_MAX_PATTERNS (128)
#define PROTRACKER_PATTERN_ROWS (64)
#define PROTRACKER_NUM_CHANNELS (4)

typedef struct __attribute__((__packed__))
{
    char name[20];
} protracker_header_t;

typedef struct __attribute__((__packed__))
{
    char name[22];
    uint16_t sample_length; // sample length in words (1 word == 2 bytes)
    uint8_t finetone;       // low nibble
    uint8_t volume;         // sample volume (0..64)
    uint16_t repeat_offset;
    uint16_t repeat_length;
} protracker_sample_t;

typedef struct __attribute__((__packed__))
{
    uint8_t length;
    uint8_t restart_position;
    uint8_t positions[PROTRACKER_MAX_PATTERNS];
} protracker_song_t;

typedef struct __attribute__((__packed__))
{
    uint8_t data[4];
} protracker_note_t;

typedef struct
{
    uint8_t high:4;
    uint8_t middle:4;
    uint8_t low:4;
} protracker_effect_t;

typedef struct __attribute__((__packed__))
{
    protracker_note_t notes[PROTRACKER_NUM_CHANNELS];
} protracker_position_t;

typedef struct __attribute__((__packed__))
{
    protracker_position_t rows[PROTRACKER_PATTERN_ROWS];
} protracker_pattern_t;

typedef struct __attribute__((__packed__))
{
    protracker_header_t header;
    protracker_sample_t sample_headers[PROTRACKER_MAX_SAMPLES];

    protracker_song_t song;

    protracker_pattern_t patterns[PROTRACKER_MAX_PATTERNS];
    uint8_t* sample_data[PROTRACKER_MAX_SAMPLES];
} protracker_t;

protracker_t* protracker_load(const char* filename);
void protracker_release(protracker_t* module);

uint8_t protracker_get_sample(const protracker_note_t* note);
uint16_t protracker_get_period(const protracker_note_t* note);
protracker_effect_t protracker_get_effect(const protracker_note_t* note);

