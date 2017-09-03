#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define PT_MAX_SAMPLES  (31)
#define PT_MAX_PATTERNS (128)
#define PT_MAX_POSITIONS (128)
#define PT_PATTERN_ROWS (64)
#define PT_NUM_CHANNELS (4)

#define PT_CMD_ARPEGGIO         (0)
#define PT_CMD_SLIDE_UP         (1)
#define PT_CMD_SLIDE_DOWN       (2)
#define PT_CMD_SLIDE_TO_NOTE    (3)
#define PT_CMD_VIBRATO          (4)
#define PT_CMD_CONTINUE_SLIDE   (5)
#define PT_CMD_CONTINUE_VIBRATO (6)
#define PT_CMD_TREMOLO          (7)
#define PT_CMD_SET_SAMPLE_OFS   (9)
#define PT_CMD_VOLUME_SLIDE     (10)
#define PT_CMD_POS_JUMP         (11)
#define PT_CMD_SET_VOLUME       (12)
#define PT_PATTERN_BREAK        (13)
#define PT_CMD_EXTENDED         (14) // Extended command
#define PT_CMD_SET_SPEED        (15)

#define PT_EXTCMD_FILTER                    (0)
#define PT_EXTCMD_FINESLIDE_UP              (1)
#define PT_EXTCMD_FINESLIDE_DOWN            (2)
#define PT_EXTCMD_SET_GLISSANDO             (3)
#define PT_EXTCMD_SET_VIBRATO_WAVEFORM      (4)
#define PT_EXTCMD_SET_FINETUNE_VALUE        (5)
#define PT_EXTCMD_LOOP_PATTERN              (6)
#define PT_EXTCMD_SET_TREMOLO_WAVEFORM      (7)
#define PT_EXTCMD_SYNC                      (8) // Unused, but value can be accessed through the replayer routine
#define PT_EXTCMD_RETRIGGER_SAMPLE          (9)
#define PT_EXTCMD_FINE_VOLUME_SLIDE_UP      (10)
#define PT_EXTCMD_FINE_VOLUME_SLIDE_DOWN    (11)
#define PT_EXTCMD_CUT_SAMPLE                (12)
#define PT_EXTCMD_DELAY_SAMPLE              (13)
#define PT_EXTCMD_DELAY_PATTERN             (14)
#define PT_EXTCMD_INVERT_LOOP               (15)

typedef struct __attribute__((__packed__))
{
    char name[20];
} protracker_header_t;

typedef struct __attribute__((__packed__))
{
    char name[22];
    uint16_t length;        // sample length in words (1 word == 2 bytes)
    uint8_t finetone;       // low nibble
    uint8_t volume;         // sample volume (0..64)
    uint16_t repeat_offset;
    uint16_t repeat_length;
} protracker_sample_t;

typedef struct __attribute__((__packed__))
{
    uint8_t length;
    uint8_t restart_position;
    uint8_t positions[PT_MAX_POSITIONS];
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
    protracker_note_t notes[PT_NUM_CHANNELS];
} protracker_position_t;

typedef struct __attribute__((__packed__))
{
    protracker_position_t rows[PT_PATTERN_ROWS];
} protracker_pattern_t;

typedef struct __attribute__((__packed__))
{
    protracker_header_t header;
    protracker_sample_t sample_headers[PT_MAX_SAMPLES];

    protracker_song_t song;

    protracker_pattern_t patterns[PT_MAX_PATTERNS];
    uint8_t* sample_data[PT_MAX_SAMPLES];
} protracker_t;

protracker_t* protracker_load(const char* filename);
int protracker_save(const protracker_t* module, const char* filename);
void protracker_free(protracker_t* module);

uint8_t protracker_get_sample(const protracker_note_t* note);
uint16_t protracker_get_period(const protracker_note_t* note);
protracker_effect_t protracker_get_effect(const protracker_note_t* note);

/**
 *
 * module - ProTracker module
 * total - if true: scan the entire pattern table
 *         if false: scan the played section of the pattern table
 *
 * returns number of patterns
 *
**/
size_t protracker_get_pattern_count(const protracker_t* module, bool total);

/**
 *
 * Remove patterns that are not included in the song
 *
**/
void protracker_remove_unused_patterns(protracker_t* module);
