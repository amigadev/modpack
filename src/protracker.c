#include "protracker.h"
#include "buffer.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

static uint16_t octaves[5][12] = {
    { 1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907 },    // 0
    {  856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453 },    // 1
    {  428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226 },    // 2
    {  214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 },    // 3
    {  107, 101,  95,  90,  85,  80,  76,  71,  67,  64,  60,  57 }     // 4
};

static char* notes[] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};

void build_note(const protracker_note_t* note, char* out)
{
    uint8_t sample = protracker_get_sample(note);
    uint16_t period = protracker_get_period(note);
    protracker_effect_t effect = protracker_get_effect(note);

    if (sample > 0 && period > 0)
    {
        for (size_t i = 0; i < 5; ++i)
        {
            for (size_t j = 0; j < 12; ++j)
            {
                if (octaves[i][j] == period)
                {
                    sprintf(out, "%s%ld%02X%1X%1X%1X", notes[j], i, sample, effect.high, effect.middle, effect.low);
                    return;
                }
            }
        }

        sprintf(out, "???%02X%1X%1X%1X", sample, effect.high, effect.middle, effect.low);
    }
    else
    {
        sprintf(out, "---%02X%1X%1X%1X", sample, effect.high, effect.middle, effect.low);
    }
}

static void process_sample_header(protracker_sample_t* sample, const uint8_t* in, size_t index)
{
    memcpy(sample, in, sizeof(protracker_sample_t));

    sample->length = htons(sample->length);
    sample->repeat_offset = htons(sample->repeat_offset);
    sample->repeat_length = htons(sample->repeat_length);

    char sample_name[sizeof(sample->name)+1];
    memset(sample_name, 0, sizeof(sample_name));
    memcpy(sample_name, sample->name, sizeof(sample->name));
    debug(" #%02u - length: $%04X, repeat offset: $%04X, repeat length: $%04X, name: '%s'\n",
        index+1,
        sample->length,
        sample->repeat_offset,
        sample->repeat_length,
        sample_name
    );
}

static const uint8_t* process_sample_data(protracker_t* module, const uint8_t* in, const uint8_t* max)
{
    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
    {
        const protracker_sample_t* sample = &(module->sample_headers[i]);

        if (in > max)
        {
            fprintf(stderr, "Premature end of data before pattern %lu.\n", i);
            break;
        }

        if (!sample->length)
        {
            continue;
        }

        size_t bytes = sample->length * 2;

        uint8_t* data = module->sample_data[i] = malloc(bytes);
        if (!data)
        {
            return NULL;
        }

        memcpy(data, in, bytes);

        debug(" #%lu - %u bytes\n", i+1, bytes);

        in += bytes;
    }

    return in;
}

protracker_t* protracker_load(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Failed top open file '%s'\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* raw = NULL;

    protracker_t module;
    memset(&module, 0, sizeof(module));

    do {
        raw = malloc(size);
        if (!raw)
        {
            fprintf(stderr, "Failed to allocate %lu bytes\n", size);
            break;
        }

        if (fread(raw, 1, size, fp) != size)
        {
            fprintf(stderr, "Failed to read %lu bytes\n", size);
            break;
        }

        const uint8_t* curr = raw;
        const uint8_t* max = curr + size;

        // Module header

        memcpy(&module.header, curr, sizeof(protracker_header_t));
        curr += sizeof(protracker_header_t);

        char mod_name[sizeof(module.header.name)+1];
        memset(mod_name, 0, sizeof(mod_name));
        memcpy(mod_name, module.header.name, sizeof(module.header.name));
        debug("Header:\n Name: '%s'\n", mod_name);

        // Sample headers

        debug("Samples:\n");
        for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
        {
            if (curr > max)
            {
                fprintf(stderr, "Premature end of data before sample %lu.\n", i);
                break;
            }

            process_sample_header(&(module.sample_headers[i]), curr, i);
            curr += sizeof(protracker_sample_t);
        }

        if (curr > max)
        {
            fprintf(stderr, "Premature end of data before song data.\n");
            break;
        }

        // Song

        memcpy(&module.song, curr, sizeof(protracker_song_t));
        curr += sizeof(protracker_song_t);

        debug("Song:\n Positions: %u (%u)\n", module.song.length, module.song.restart_position);

        debug(" Patterns:");
        bool positions_valid = true;
        for (size_t i = 0; i < module.song.length; ++i)
        {
            debug(" %u", module.song.positions[i]);

            if (module.song.positions[i] > 127)
            {
                fprintf(stderr, "Invalid song pattern at position %lu", i+1);
                positions_valid = false;
                break;
            }
        }
        debug("\n");
        if (!positions_valid)
        {
            break;
        }

        if (memcmp("M.K.", curr, 4) && memcmp("M!K!", curr, 4) && memcmp("FLT4", curr, 4) && memcmp("4CHN", curr, 4))
        {
            fprintf(stderr, "Could not find magic word, is this a ProTracker module?\n");
            break;
        }
        curr += 4;

        // Patterns

        for (size_t i = 0, n = protracker_get_pattern_count(&module, true); i < n; ++i)
        {
            if (curr > max)
            {
                fprintf(stderr, "Premature end of data before pattern %lu.\n", i);
                break;
            }

            memcpy(&module.patterns[i], curr, sizeof(protracker_pattern_t));

            debug("Pattern #%lu:\n", i);
            for(size_t j = 0; j < PT_PATTERN_ROWS; ++j)
            {
                const protracker_pattern_row_t* pos = &(module.patterns[i].rows[j]);

                debug(" #%02lu:", j);

                for(size_t k = 0; k < PT_NUM_CHANNELS; ++k)
                {
                    const protracker_note_t* note = &(pos->notes[k]);

                    char note_string[32];
                    char effect_string[8];

                    build_note(note, note_string);

                    debug(" %s", note_string);
                }

                debug("\n");
            }

            curr += sizeof(protracker_pattern_t);
        }

        debug("Sample Data:\n");
        const uint8_t* end = process_sample_data(&module, curr, max);

        if (!end)
        {
            fprintf(stderr, "Failed to load sample data.\n");
            break;
        }

        debug("Protracker module loaded successfully. (%ld)\n", max - end);

        protracker_t* output = malloc(sizeof(protracker_t));
        if (!output)
        {
            fprintf(stderr, "Failed to allocate module block");
            break;
        }
        *output = module;

        free(raw);

        return output;
    } while (0);

    fprintf(stderr, "Failed to load Protracker module.\n");

    free(raw);
    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
    {
        free(module.sample_data[i]);
    }

    return NULL;
}

int protracker_convert(buffer_t* buffer, const protracker_t* module)
{
    buffer_add(buffer, &(module->header), sizeof(protracker_header_t));
    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
    {
        protracker_sample_t sample = module->sample_headers[i];

        sample.length = htons(sample.length);
        sample.repeat_offset = htons(sample.repeat_offset);
        sample.repeat_length = htons(sample.repeat_length);

        buffer_add(buffer, &sample, sizeof(protracker_sample_t));
    }

    buffer_add(buffer, &(module->song), sizeof(protracker_song_t));
    buffer_add(buffer, "M.K.", 4);

    for (size_t i = 0, n = protracker_get_pattern_count(module, true); i < n; ++i)
    {
        const protracker_pattern_t* pattern = &(module->patterns[i]);
        buffer_add(buffer, pattern, sizeof(protracker_pattern_t));
    }

    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
    {
        const protracker_sample_t* sample = &(module->sample_headers[i]);

        if (!sample->length)
        {
            continue;
        }

        buffer_add(buffer, module->sample_data[i], sample->length * 2);
    }

    return 0;
}

void protracker_free(protracker_t* module)
{
    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i)
    {
        free(module->sample_data[i]);
    }
    free(module);
}

uint8_t protracker_get_sample(const protracker_note_t* note)
{
    return (note->data[0] & 0xf0) | ((note->data[2] & 0xf0) >> 4);

}

uint16_t protracker_get_period(const protracker_note_t* note)
{
    return ((note->data[0] & 15) << 8) + note->data[1];
}

protracker_effect_t protracker_get_effect(const protracker_note_t* note)
{
    protracker_effect_t effect = {
        (note->data[2] & 0x0f),
        (note->data[3] & 0xf0) >> 4,
        note->data[3] & 0x0f
    };

    return effect;
}

void protracker_set_sample(protracker_note_t* note, uint8_t sample)
{
    note->data[0] = (note->data[0] & 0x0f) | (sample & 0xf0);
    note->data[2] = (note->data[2] & 0x0f) | ((sample & 0x0f) << 4);
}

size_t protracker_get_pattern_count(const protracker_t* module, bool total)
{
    uint8_t max_pattern = 0;

    if (total)
    {
        for (uint8_t i = 0; i < PT_MAX_POSITIONS; ++i)
        {
            if (module->song.positions[i] > max_pattern)
                max_pattern = module->song.positions[i];
        }
    }
    else
    {
        for (uint8_t i = 0, n = module->song.length; i < n; ++i)
        {
            if (module->song.positions[i] > max_pattern)
                max_pattern = module->song.positions[i];
        }
    }

    return max_pattern+1;
}

void protracker_remove_unused_patterns(protracker_t* module)
{
    size_t used_patterns = protracker_get_pattern_count(module, false);
    size_t total_patterns = protracker_get_pattern_count(module, true);

    debug("Removing unused patterns...\n");

    for (size_t i = 0; i < used_patterns; ++i)
    {
        bool used = false;
        for (size_t j = 0, m = module->song.length; j < m; ++j)
        {
            if (module->song.positions[j] == i)
            {
                used = true;
                break;
            }
        }

        if (used)
        {
            continue;
        }

        debug(" #%u - not used, removing...\n", i);

        // move pattern data up

        if (i < total_patterns)
        {
            memcpy(&(module->patterns[i]), &(module->patterns[i+1]), (total_patterns-(i+1)) * sizeof(protracker_pattern_t));
        }

        // fixup song positions

        for (uint8_t j = 0, m = module->song.length; j < m; ++j)
        {
            if (module->song.positions[j] > i)
            {
                --module->song.positions[j];
            }
        }

        --used_patterns;
        --total_patterns;
    }
}

typedef struct
{
    uint8_t sample;
    bool found;
} sample_index_search;

static void match_sample_index(const protracker_note_t* note, uint8_t channel, void* data)
{
    sample_index_search* internal = (sample_index_search*)data;
    if (internal->sample == protracker_get_sample(note))
    {
        internal->found = true;
    }
}

void protracker_remove_unused_samples(protracker_t* module)
{
    debug("Removing unused samples...\n");

    for (size_t i = 0, n = PT_MAX_SAMPLES; i < n; ++i)
    {
        uint8_t sample = (uint8_t)i+1;
        sample_index_search search_data = { sample, false };

        protracker_scan_notes(module, match_sample_index, &search_data);

        if (search_data.found)
        {
            continue;
        }

        debug(" #%lu - not used, removing...\n", (i+1));

        free(module->sample_data[i]);
        module->sample_data[i] = NULL;
        module->sample_headers[i].length = 0;
        module->sample_headers[i].repeat_offset = 0;
        module->sample_headers[i].repeat_offset = 0;
    }
}

typedef struct
{
    uint8_t source;
    uint8_t dest;
} compact_sample_filter;

static void compact_sample_index(protracker_note_t* note, uint8_t channel, void* data)
{
    compact_sample_filter* internal = (compact_sample_filter*)data;
    uint8_t sample = protracker_get_sample(note);

    if (sample == internal->source)
    {
        protracker_set_sample(note, internal->dest);
    }
}

void protracker_compact_sample_indexes(protracker_t* module)
{
    debug("Compacting sample indexes...\n");

    bool used[PT_MAX_SAMPLES] = { 0 };

    for (size_t i = 0, n = PT_MAX_SAMPLES; i < n; ++i)
    {
        uint8_t sample = (uint8_t)i+1;
        sample_index_search search_data = { sample, false };

        protracker_scan_notes(module, match_sample_index, &search_data);

        if (search_data.found)
        {
            used[i] = true;
            continue;
        }
    }

    size_t source_index = 0;
    for (size_t i = 0; i < PT_MAX_SAMPLES; ++i, ++source_index)
    {
        if (!used[i])
        {
            debug(" #%lu - compacting index.\n", (i+1));
            ++ source_index;
        }

        compact_sample_filter compact_data = {
            (uint8_t)(source_index+1),
            (uint8_t)(i+1)
        };

        protracker_transform_notes(module, compact_sample_index, &compact_data);

        if (i != source_index)
        {
            if (source_index < PT_MAX_SAMPLES)
            {
                module->sample_headers[i] = module->sample_headers[source_index];
                module->sample_data[i] = module->sample_data[source_index];
            }
            else
            {
                memset(&(module->sample_headers[i]), 0, sizeof(protracker_sample_t));
                module->sample_data[i] = NULL;
            }

        }
    }
}

void protracker_transform_notes(protracker_t* module, void (*transform)(protracker_note_t*, uint8_t channel, void* data), void* data)
{
    for (size_t i = 0, n = module->song.length; i < n; ++i)
    {
        protracker_pattern_t* pattern = &(module->patterns[module->song.positions[i]]);

        for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
        {
            protracker_pattern_row_t* row = &(pattern->rows[j]);

            for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
            {
                protracker_note_t* note = &(row->notes[k]);

                transform(note, k, data);
            }
        }
    }
}

void protracker_scan_notes(const protracker_t* module, void (*scan)(const protracker_note_t*, uint8_t channel, void* data), void* data)
{
    for (size_t i = 0, n = module->song.length; i < n; ++i)
    {
        const protracker_pattern_t* pattern = &(module->patterns[module->song.positions[i]]);

        for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
        {
            const protracker_pattern_row_t* row = &(pattern->rows[j]);

            for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
            {
                const protracker_note_t* note = &(row->notes[k]);

                scan(note, k, data);
            }
        }
    }
}