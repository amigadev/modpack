#include "player61a.h"
#include "options.h"
#include "log.h"

/*
		;0 = two files (song + samples)
		;1 = sign 'P61A'
		;2 = no samples
		;3 = tempo
		;4 = icon
		;5 = delta - 8 bit delta
		;6 = sample packing - 4 bit delta

*/

/*

    0 / 4: P61A
    4 / 2: ???
    7 / 1: Number of patterns?

    4 / 2: Sample buffer offset


*/

#include <string.h>

static const char* signature = "P61A";

static void build_samples(player61a_t* output, const protracker_t* module, const char* options)
{
    LOG_DEBUG("Building sample table:\n");

    bool usage[PT_NUM_SAMPLES];
    size_t sample_count = protracker_get_used_samples(module, usage);

    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        const protracker_sample_t* input = &(module->sample_headers[i]);
        player61a_sample_t* sample = &(output->sample_headers[i]);

        if (!usage[i])
        {
            continue;
        }

        if (input->repeat_length > 1)
        {
            // looping

            uint16_t length = input->repeat_offset + input->repeat_length;
            LOG_TRACE(" #%lu - %u bytes (looped)\n", (i+1), length * 2);

            if (length != input->length)
            {
                LOG_WARN("Looped sample #%lu truncated (%u -> %u bytes).\n", (i+1), input->length * 2, length * 2);
            }

            sample->length = length;
            sample->finetone = input->finetone;
            sample->volume = input->volume > 64 ? 64 : input->volume;
            sample->repeat_offset = input->repeat_offset;
        }
        else
        {
            // not looping

            LOG_TRACE(" #%lu - %u bytes\n", (i+1), input->length * 2);

            sample->length = input->length;
            sample->finetone = input->finetone;
            sample->volume = input->volume > 64 ? 64 : input->volume;
            sample->repeat_offset = 0xffff;
        }
    }

    LOG_DEBUG(" %lu samples used.\n", sample_count);

    output->header.sample_count = (uint8_t)sample_count;
}

static void build_patterns(player61a_t* output, const protracker_t* input, const char* options)
{
    output->header.num_patterns = input->num_patterns;

    output->song.length = input->song.length;
    memcpy(output->song.positions, input->song.positions, sizeof(uint8_t) * PT_NUM_POSITIONS);

    output->pattern_offsets = malloc(input->num_patterns * sizeof(player61a_pattern_offset_t));
    memset(output->pattern_offsets, 0, input->num_patterns * sizeof(player61a_pattern_offset_t));
}

static void player61a_destroy(const player61a_t* module)
{
    free(module->pattern_offsets);
}

#if 0

static int8_t deltas[] = {
    0,1,2,4,8,16,32,64,128,-64,-32,-16,-8,-4,-2,-1
};

static uint16_t periods[] = {
    856,808,762,720,678,640,604,570,538,508,480,453,    // octave 1
    428,404,381,360,339,320,302,285,269,254,240,226,    // octave 2
    214,202,190,180,170,160,151,143,135,127,120,113     // octave 3
};

static uint8_t get_period_index(uint16_t period)
{
    for (size_t i = 0; i < (sizeof(periods) / sizeof(uint16_t)); ++i)
    {
        if (periods[i] == period)
        {
            return (uint8_t)(i+1);
        }
    }
    return 0;
}

/*

* P61 Pattern Format:

* o = If set compression info follows
* n = Note (6 bits)
* i = Instrument (5 bits)
* c = Command (4 bits)
* b = Info byte (8 bits)

* onnnnnni iiiicccc bbbbbbbb	Note, instrument and command
* o110cccc bbbbbbbb		Only command
* o1110nnn nnniiiii		Note and instrument
* o1111111			Empty note

* Compression info:

* 00nnnnnn		 	n empty rows follow
* 10nnnnnn		 	n same rows follow (for faster testing)
* 01nnnnnn oooooooo		Jump o (8 bit offset) bytes back for n rows
* 11nnnnnn oooooooo oooooooo	Jump o (16 bit offset) bytes back for n rows

*/

#endif

static void write_song(buffer_t* buffer, const player61a_t* module, const char* options)
{
    if (has_option(options, "sign", false))
    {
        LOG_TRACE(" - Adding signature.\n");
        buffer_add(buffer, signature, strlen(signature));
    }

    // header

    {
        player61a_header_t header;

        header.sample_offset = htons(module->header.sample_offset);
        header.num_patterns = module->header.num_patterns;
        header.sample_count = module->header.sample_count;

        buffer_add(buffer, &header, sizeof(header));
    }

    // sample headers

    for (size_t i = 0; i < module->header.sample_count; ++i)
    {
        const player61a_sample_t* in = &(module->sample_headers[i]);
        player61a_sample_t sample;

        sample.length = htons(in->length);
        sample.finetone = in->finetone;
        sample.volume = in->volume;
        sample.repeat_offset = htons(in->repeat_offset);

        buffer_add(buffer, &sample, sizeof(sample));
    }

    // pattern offsets

    for (size_t i = 0; i < module->header.num_patterns; ++i)
    {
        player61a_pattern_offset_t offset;
        for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
        {
            offset.channels[j] = htons(module->pattern_offsets[i].channels[j]);
        }

        buffer_add(buffer, &offset, sizeof(offset));
    }

    // tune positions

    {
        buffer_add(buffer, module->song.positions, module->song.length);

        uint8_t temp = 0xff;
        buffer_add(buffer, &temp, sizeof(temp));
    }
}

static void write_samples(buffer_t* buffer, const player61a_t* module)
{
}

bool player61a_convert(buffer_t* buffer, const protracker_t* module, const char* options)
{
    LOG_INFO("Converting to The Player 6.1A...\n");

    player61a_t temp = { 0 };

    build_samples(&temp, module, options);
    build_patterns(&temp, module, options);

    if (has_option(options, "song", true))
    {
        LOG_DEBUG(" - Writing song data...\n");
        write_song(buffer, &temp, options);
    }

    if (has_option(options, "samples", true))
    {
        LOG_DEBUG(" - Writing sample data...\n");
        write_samples(buffer, &temp);
    }

    player61a_destroy(&temp);

    return true;
}
