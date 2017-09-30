#include "../include/internal/protracker.h"
#include "../include/internal/buffer.h"
#include "../include/internal/options.h"
#include "../include/internal/log.h"

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

void protracker_channel_to_text(const protracker_channel_t* channel, char* out, size_t buflen)
{
    uint8_t sample = protracker_get_sample(channel);
    uint16_t period = protracker_get_period(channel);
    protracker_effect_t effect = protracker_get_effect(channel);

    if (sample > 0 && period > 0)
    {
        for (size_t i = 0; i < 5; ++i)
        {
            for (size_t j = 0; j < 12; ++j)
            {
                if (octaves[i][j] == period)
                {
                    snprintf(out, buflen, "%s%ld%02X%1X%1X%1X", notes[j], i, sample, effect.cmd, effect.data.ext.cmd, effect.data.ext.value);
                    return;
                }
            }
        }

        snprintf(out, buflen, "???%02X%1X%1X%1X", sample, effect.cmd, effect.data.ext.cmd, effect.data.ext.value);
    }
    else
    {
        snprintf(out, buflen, "---%02X%1X%1X%1X", sample, effect.cmd, effect.data.ext.cmd, effect.data.ext.value);
    }
}

static void process_sample_header(protracker_sample_t* sample, const uint8_t* in, size_t index)
{
    memcpy(sample, in, sizeof(protracker_sample_t));

    sample->length = ntohs(sample->length);
    sample->repeat_offset = ntohs(sample->repeat_offset);
    sample->repeat_length = ntohs(sample->repeat_length);

    char sample_name[sizeof(sample->name)+1];
    memset(sample_name, 0, sizeof(sample_name));
    memcpy(sample_name, sample->name, sizeof(sample->name));
    LOG_TRACE(instance, " #%02u - length: $%04X, repeat offset: $%04X, repeat length: $%04X, name: '%s'\n",
        index+1,
        sample->length,
        sample->repeat_offset,
        sample->repeat_length,
        sample_name
    );
}

static const uint8_t* process_sample_data(modpack_t* instance, protracker_t* module, const uint8_t* in, const uint8_t* max)
{
    size_t i;
    for (i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        const protracker_sample_t* sample = &(module->sample_headers[i]);

        if (in > max)
        {
            LOG_ERROR(instance, "Premature end of data before sample #%lu.\n", (i+1));
            break;
        }

        if (!sample->length)
        {
            continue;
        }

        size_t bytes = sample->length * 2;

        uint8_t* data = module->sample_data[i] = instance->alloc(bytes);
        if (!data)
        {
            return NULL;
        }

        memcpy(data, in, bytes);

        for (size_t i = 0; i < 256 && i < bytes; ++i)
        {
            if ((i & 15) == 0)
            {
                LOG_TRACE(instance, "\n%04X:", i);
            }

            LOG_TRACE(instance, "%02X", data[i]);
        }
        LOG_TRACE(instance, "\n");

        LOG_TRACE(instance, " #%lu - %u bytes\n", i+1, bytes);

        in += bytes;
    }

    return (i == PT_NUM_SAMPLES) ? in : NULL;
}

protracker_t* protracker_load(modpack_t* instance, const modpack_buffer_t* buffer)
{
    LOG_DEBUG(instance, "Loading Protracker module...\n");

    protracker_t module;
    protracker_create(&module);

    do
    {
        size_t size = buffer_count(buffer);
        if (size < sizeof(protracker_header_t))
        {
            LOG_ERROR(instance, "Premature end of data before header.\n");
            break;
        }
        const uint8_t* raw = buffer_get(buffer, 0);

        const uint8_t* curr = raw;
        const uint8_t* max = curr + size;

        // Module header

        memcpy(&module.header, curr, sizeof(protracker_header_t));
        curr += sizeof(protracker_header_t);

        char mod_name[sizeof(module.header.name)+1];
        memset(mod_name, 0, sizeof(mod_name));
        memcpy(mod_name, module.header.name, sizeof(module.header.name));
        LOG_TRACE(instance, "Header:\n Name: '%s'\n", mod_name);

        // Sample headers

        LOG_TRACE(instance, "Samples:\n");
        for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
        {
            if (curr > max)
            {
                LOG_ERROR(instance, "Premature end of data before sample %lu.\n", i);
                break;
            }

            process_sample_header(&(module.sample_headers[i]), curr, i);
            curr += sizeof(protracker_sample_t);
        }

        if (curr > max)
        {
            LOG_ERROR(instance, "Premature end of data before song data.\n");
            break;
        }

        // Song

        memcpy(&module.song, curr, sizeof(protracker_song_t));
        curr += sizeof(protracker_song_t);

        LOG_TRACE(instance, "Song:\n Positions: %u (%u)\n", module.song.length, module.song.restart_position);

        LOG_TRACE(instance, " Patterns:");
        bool positions_valid = true;
        uint8_t max_pattern = 0;
        for (size_t i = 0; i < PT_NUM_POSITIONS; ++i)
        {
            uint8_t pattern_index = module.song.positions[i];
            max_pattern = max_pattern < pattern_index ? pattern_index : max_pattern;
            LOG_TRACE(instance, " %u", pattern_index);
        }
        LOG_TRACE(instance, "\n");
        if (!positions_valid)
        {
            break;
        }

        if (memcmp("M.K.", curr, 4) && memcmp("M!K!", curr, 4) && memcmp("FLT4", curr, 4) && memcmp("4CHN", curr, 4))
        {
            LOG_ERROR(instance, "Could not find magic word, is this a ProTracker module?\n");
            break;
        }
        curr += 4;

        // Patterns

        module.num_patterns = max_pattern + 1;
        module.patterns = instance->alloc(module.num_patterns * sizeof(protracker_pattern_t));

        for (size_t i = 0; i < module.num_patterns; ++i)
        {
            if (curr > max)
            {
                LOG_ERROR(instance, "Premature end of data before pattern %lu.\n", i);
                break;
            }

            memcpy(&module.patterns[i], curr, sizeof(protracker_pattern_t));

            LOG_TRACE(instance, "Pattern #%lu:\n", i);
            for(size_t j = 0; j < PT_PATTERN_ROWS; ++j)
            {
                const protracker_pattern_row_t* pos = &(module.patterns[i].rows[j]);

                LOG_TRACE(instance, " #%02lu:", j);

                for(size_t k = 0; k < PT_NUM_CHANNELS; ++k)
                {
                    const protracker_channel_t* channel = &(pos->channels[k]);

                    char channel_string[32];

                    protracker_channel_to_text(channel, channel_string, sizeof(channel_string));

                    LOG_TRACE(instance, " %s", channel_string);
                }

                LOG_TRACE(instance, "\n");
            }

            curr += sizeof(protracker_pattern_t);
        }

        LOG_TRACE(instance, "Sample Data:\n");
        const uint8_t* end = process_sample_data(&module, curr, max);

        if (!end)
        {
            LOG_ERROR(instance, "Failed to load sample data.\n");
            break;
        }

        if (max < end)
        {
            LOG_WARN(instance, "%lu bytes not consumed while loading.\n", end - max);
        }
        else if (max > end)
        {
            LOG_ERROR(instance, "%lu bytes missing while loading.\n", max - end);
            break;
        }

        LOG_DEBUG(instance, "Protracker module loaded successfully.\n");

        protracker_t* output = instance->alloc(sizeof(protracker_t));
        if (!output)
        {
            LOG_ERROR(instance, "Failed to allocate module block");
            break;
        }
        *output = module;

        return output;
    } while (0);

    LOG_ERROR(instance, "Failed to load Protracker module.\n");

    protracker_destroy(&module);

    return NULL;
}

bool protracker_convert(modpack_buffer_t* buffer, const protracker_t* module, const char* options)
{
    LOG_INFO(instance, "Exporting ProTracker module\n");

    LOG_TRACE(instance, " - Header\n");

    buffer_add(buffer, &(module->header), sizeof(protracker_header_t));

    LOG_TRACE(instance, " - Samples\n");

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        protracker_sample_t sample = module->sample_headers[i];

        sample.length = htons(sample.length);
        sample.repeat_offset = htons(sample.repeat_offset);
        sample.repeat_length = htons(sample.repeat_length);

        buffer_add(buffer, &sample, sizeof(protracker_sample_t));
    }

    LOG_TRACE(instance, " - Song\n");

    buffer_add(buffer, &(module->song), sizeof(protracker_song_t));
    buffer_add(buffer, "M.K.", 4);

    LOG_TRACE(instance, " - Patterns (%lu)\n", module->num_patterns);

    for (size_t i = 0; i < module->num_patterns; ++i)
    {
        const protracker_pattern_t* pattern = &(module->patterns[i]);
        buffer_add(buffer, pattern, sizeof(protracker_pattern_t));
    }

    LOG_TRACE(instance, " - Sample Data\n");

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        const protracker_sample_t* sample = &(module->sample_headers[i]);

        if (!sample->length)
        {
            continue;
        }

        buffer_add(buffer, module->sample_data[i], sample->length * 2);
    }

    return true;
}

void protracker_create(protracker_t* module)
{
    memset(module, 0, sizeof(protracker_t));
}

void protracker_destroy(modpack_t* instance, protracker_t* module)
{
    instance->free(module->patterns);
    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        instance->free(module->sample_data[i]);
    }
}

void protracker_free(modpack_t* instance, protracker_t* module)
{
    protracker_destroy(instance, module);
    instance->free(module);
}

uint8_t protracker_get_sample(const protracker_channel_t* channel)
{
    return (channel->data[0] & 0x10) | ((channel->data[2] & 0xf0) >> 4);

}

uint16_t protracker_get_period(const protracker_channel_t* channel)
{
    return ((channel->data[0] & 0x0f) << 8) + channel->data[1];
}

protracker_effect_t protracker_get_effect(const protracker_channel_t* channel)
{
    protracker_effect_t effect;

    effect.cmd = (channel->data[2] & 0x0f);
    effect.data.value = channel->data[3];

    return effect;
}

void protracker_set_sample(protracker_channel_t* channel, uint8_t sample)
{
    channel->data[0] = (channel->data[0] & 0x0f) | (sample & 0x10);
    channel->data[2] = (channel->data[2] & 0x0f) | ((sample & 0x0f) << 4);
}

void protracker_set_period(protracker_channel_t* channel, uint16_t period)
{
    channel->data[0] = (channel->data[0] & 0xf0) | ((period & 0x0f00) >> 8);
    channel->data[1] = period & 0x00ff;
}

void protracker_set_effect(protracker_channel_t* channel, const protracker_effect_t* effect)
{
    channel->data[2] = (channel->data[2] & 0xf0) | effect->cmd;
    channel->data[3] = effect->data.value;
}

typedef struct
{
    bool* usage;
} sample_usage_data;

void sample_usage_filter(const protracker_channel_t* channel, uint8_t index, void* data)
{
    sample_usage_data* internal = (sample_usage_data*)data;
    uint8_t sample = protracker_get_sample(channel);

    if (sample > 0)
    {
        internal->usage[sample - 1] = true;
    }
}

size_t protracker_get_used_samples(const protracker_t* module, bool* usage)
{
    memset(usage, 0, sizeof(bool) * PT_NUM_SAMPLES);

    sample_usage_data usage_data = { usage };
    protracker_scan_notes(module, sample_usage_filter, &usage_data);

    size_t count = 0;
    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        if (usage[i])
        {
            ++ count;
        }
    }

    return count;
}

size_t protracker_get_pattern_count(const protracker_t* module)
{
    uint8_t max_pattern = 0;

    for (uint8_t i = 0; i < module->song.length; ++i)
    {
        if (module->song.positions[i] > max_pattern)
            max_pattern = module->song.positions[i];
    }

    return max_pattern+1;
}

void protracker_remove_unused_patterns(protracker_t* module)
{
    size_t used_patterns = protracker_get_pattern_count(module);
    size_t total_patterns = module->num_patterns;

    LOG_DEBUG(instance, "Removing unused patterns...\n");

    for (size_t i = module->song.length; i < PT_NUM_POSITIONS; ++i)
    {
        module->song.positions[i] = 0;
    }

    size_t num_patterns = 0;
    for (size_t i = 0; i < module->num_patterns; ++i)
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

        size_t pattern_index = num_patterns;
        if (!used)
        {
            LOG_TRACE(instance, " #%lu - not used, removing...\n", i);
        }
        else
        {
            ++ num_patterns;
        }

        if (pattern_index == i)
        {
            continue;
        }

        module->patterns[pattern_index] = module->patterns[i];

        for (size_t j = 0; j < PT_NUM_POSITIONS; ++j)
        {
            size_t curr_index = module->song.positions[j];
            if (curr_index == i)
            {
                module->song.positions[j] -= (i - pattern_index);
            }
        }
    }

    module->num_patterns = num_patterns;
}

void protracker_remove_unused_samples(protracker_t* module)
{
    LOG_DEBUG(instance, "Removing unused samples...\n");

    bool used[PT_NUM_SAMPLES];
    size_t sample_count = protracker_get_used_samples(module, used);

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        if (used[i] || !module->sample_headers[i].length)
        {
            continue;
        }

        LOG_TRACE(instance, " #%lu - not used, removing...\n", (i+1));

        instance->free(module->sample_data[i]);
        module->sample_data[i] = NULL;
        module->sample_headers[i].length = 0;
        module->sample_headers[i].repeat_offset = 0;
        module->sample_headers[i].repeat_length = 0;
    }
}

void protracker_trim_samples(protracker_t* module)
{
    LOG_DEBUG(instance, "Trimming samples...\n");

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        protracker_sample_t* sample = &(module->sample_headers[i]);
        if (!sample->length || sample->repeat_length > 1)
        {
            continue;
        }

        const uint8_t* data = module->sample_data[i];
        ssize_t sample_end;

        for (sample_end = (sample->length) - 1; sample_end >= 0; --sample_end)
        {
            size_t byte_offset = sample_end << 1;
            if (data[byte_offset] || data[byte_offset+1])
            {
                break;
            }
        }

        size_t sample_length = (sample_end + 1);
        if (sample_length == sample->length)
        {
            continue;
        }

        LOG_TRACE(instance, " #%lu - %lu -> %lu bytes (%lu bytes saved)\n", (i + 1), sample->length * 2, sample_length * 2, (sample->length - sample_length) * 2);
        sample->length = sample_length;
    }
}

void protracker_clean_effects(protracker_t* module, const char* options)
{
    LOG_DEBUG(instance, "Cleaning effects...\n");

    bool clean_e8 = has_option(options, "clean:e8", false);

    for (size_t i = 0; i < module->num_patterns; ++i)
    {
        protracker_pattern_t* pattern = &(module->patterns[i]);

        for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
        {
            protracker_pattern_row_t* row = &(pattern->rows[j]);

            bool has_break = false;
            for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
            {
                protracker_channel_t* channel = &(row->channels[k]);
                protracker_effect_t effect = protracker_get_effect(channel);

                switch (effect.cmd)
                {
                    case PT_CMD_POS_JUMP:
                    case PT_CMD_PATTERN_BREAK:
                    {
                        if (has_break)
                        {
                            LOG_TRACE(instance, " (P:%lu,R:%lu,C:%lu) - Removed POS. JUMP/PAT. BREAK\n", i, j, k);
                            memset(&effect, 0, sizeof(effect));
                        }
                        has_break = true;
                    }
                    break;

                    case PT_CMD_EXTENDED:
                    {
                        switch (effect.data.ext.cmd)
                        {
                            case PT_ECMD_E8:
                            {
                                if (clean_e8)
                                {
                                    LOG_TRACE(instance, " (P:%lu,R:%lu,C:%lu) - Removed E8x\n", i, j, k);
                                    memset(&effect, 0, sizeof(effect));
                                }
                            }
                            break;
                        }
                    }
                    break;
                }

                protracker_set_effect(channel, &effect);
            }
        }
    }
}


typedef struct
{
    uint8_t src, dest;
} sample_replace_data;

void sample_replace_filter(protracker_channel_t* channel, uint8_t index, void* data)
{
    sample_replace_data* internal = (sample_replace_data*)data;
    uint8_t sample = protracker_get_sample(channel);
    if (sample == internal->src)
    {
        protracker_set_sample(channel, internal->dest);
    }
}

void protracker_remove_identical_samples(protracker_t* module)
{
    LOG_DEBUG(instance, "Removing identical samples...\n");

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        const protracker_sample_t* src = &(module->sample_headers[i]);

        if (!src->length)
        {
            continue;
        }

        for (size_t j = i + 1; j < PT_NUM_SAMPLES; ++j)
        {
            protracker_sample_t* dest = &(module->sample_headers[j]);

            if (
                (src->length != dest->length) ||
                (src->finetone != dest->finetone) ||
                (src->volume != dest->volume) ||
                (src->repeat_offset != dest->repeat_offset) ||
                (src->repeat_length != dest->repeat_length)
            )
            {
                continue;
            }

            if (memcmp(module->sample_data[i], module->sample_data[j], src->length * 2))
            {
                continue;
            }

            LOG_TRACE(instance, " #%lu equals #%lu, merging...\n", (i+1), (j+1));

            sample_replace_data replace_data = {
                (j+1), (i+1)
            };

            protracker_transform_notes(module, sample_replace_filter, &replace_data);

            instance->free(module->sample_data[j]);
            module->sample_data[j] = NULL;
            module->sample_headers[j].length = 0;
            module->sample_headers[j].repeat_offset = 0;
            module->sample_headers[j].repeat_length = 0;
        }
    }
}

typedef struct
{
    uint8_t sample, delta;
} compact_sample_data;

static void compact_sample_filter(protracker_channel_t* channel, uint8_t index, void* data)
{
    compact_sample_data* internal = (compact_sample_data*)data;
    uint8_t sample = protracker_get_sample(channel);

    if (sample == internal->sample)
    {
        protracker_set_sample(channel, sample-internal->delta);
    }
}

void protracker_compact_sample_indexes(protracker_t* module)
{
    LOG_DEBUG(instance, "Compacting sample indexes...\n");

    bool used[PT_NUM_SAMPLES];
    size_t sample_count = protracker_get_used_samples(module, used);

    for (size_t i = 0, sample_offset = 0; i < PT_NUM_SAMPLES; ++i)
    {
        size_t sample_index = sample_offset;
        bool remove = false;
        if (used[i])
        {
            ++ sample_offset;
        }
        else
        {
            bool existing = false;
            for (size_t j = i + 1; j < PT_NUM_SAMPLES; ++j)
            {
                if (module->sample_headers[j].length)
                {
                    existing = true;
                    break;
                }
            }

            if (existing)
            {
                LOG_TRACE(instance, " #%lu - not used, compacting.\n", (i+1));
            }
        }

        if (sample_index == i)
        {
            continue;
        }

        memcpy(&(module->sample_headers[sample_index]), &(module->sample_headers[i]), sizeof(protracker_sample_t));
        module->sample_data[sample_index] = module->sample_data[i];

        compact_sample_data compact_data = { (uint8_t)(i+1), i - sample_index };
        protracker_transform_notes(module, compact_sample_filter, &compact_data);
    }

    for (size_t i = sample_count; i < PT_NUM_SAMPLES; ++i)
    {
        memset(&(module->sample_headers[i]), 0, sizeof(protracker_sample_t));
        module->sample_data[i] = 0;
    }
}

void protracker_transform_notes(protracker_t* module, void (*transform)(protracker_channel_t*, uint8_t index, void* data), void* data)
{
    for (size_t i = 0, n = module->song.length; i < n; ++i)
    {
        protracker_pattern_t* pattern = &(module->patterns[module->song.positions[i]]);

        for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
        {
            protracker_pattern_row_t* row = &(pattern->rows[j]);

            for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
            {
                protracker_channel_t* channel = &(row->channels[k]);

                transform(channel, k, data);
            }
        }
    }
}

void protracker_scan_notes(const protracker_t* module, void (*scan)(const protracker_channel_t*, uint8_t index, void* data), void* data)
{
    for (size_t i = 0, n = module->song.length; i < n; ++i)
    {
        const protracker_pattern_t* pattern = &(module->patterns[module->song.positions[i]]);

        for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
        {
            const protracker_pattern_row_t* row = &(pattern->rows[j]);

            for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
            {
                const protracker_channel_t* channel = &(row->channels[k]);

                scan(channel, k, data);
            }
        }
    }
}