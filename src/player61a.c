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

        uint16_t length;
        if (input->repeat_length > 1)
        {
            // looping

            length = input->repeat_offset + input->repeat_length;
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

            length = input->length;
            LOG_TRACE(" #%lu - %u bytes\n", (i+1), input->length * 2);

            sample->length = length;
            sample->finetone = input->finetone;
            sample->volume = input->volume > 64 ? 64 : input->volume;
            sample->repeat_offset = 0xffff;
        }

        // TODO: compression / delta encoding

        buffer_add(&(output->samples), module->sample_data[i], length * 2);
    }

    LOG_DEBUG(" %lu samples used.\n", sample_count);

    output->header.sample_count = (uint8_t)sample_count;
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


#define CHANNEL_ALL             (0x00)
#define CHANNEL_COMMAND         (0x60)
#define CHANNEL_NOTE_INSTRUMENT (0x70)
#define CHANNEL_EMPTY           (0x7f)

static uint16_t periods[] = {
    856,808,762,720,678,640,604,570,538,508,480,453,    // octave 1
    428,404,381,360,339,320,302,285,269,254,240,226,    // octave 2
    214,202,190,180,170,160,151,143,135,127,120,113     // octave 3
};

static uint8_t get_note_index(uint16_t period)
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

static size_t convert_channel(player61a_channel_t* out, const protracker_channel_t* in, const protracker_pattern_row_t* row, size_t channel_index, uint32_t* usecode)
{
    uint8_t instrument = protracker_get_sample(in);
    uint16_t period = protracker_get_period(in);
    protracker_effect_t effect = protracker_get_effect(in);

    uint8_t note = get_note_index(period);

    bool has_command = (effect.cmd + effect.data.value) != 0;
    switch (effect.cmd)
    {
        case PT_CMD_ARPEGGIO:
        {
            if (effect.data.value)
            {
                effect.cmd = PT_CMD_8; // P61A uses 8 for arpeggio
            }
        }
        break;

        case PT_CMD_SLIDE_UP:
        case PT_CMD_SLIDE_DOWN:
        {
            has_command = effect.data.value != 0;
        }
        break;

        // TODO: these commands need processing
        case PT_CMD_CONTINUE_SLIDE:
        case PT_CMD_CONTINUE_VIBRATO:
        case PT_CMD_VOLUME_SLIDE:
        case PT_CMD_POS_JUMP:
        case PT_CMD_PATTERN_BREAK:
        {
        }
        break;

        case PT_CMD_SET_VOLUME:
        {
            effect.data.value = effect.data.value > 64 ? 64 : effect.data.value;
        }
        break;

        case PT_CMD_8:
        {
            effect.cmd = PT_CMD_EXTENDED;
            effect.data.ext.cmd = PT_ECMD_E8;
        }
        break;

        case PT_CMD_EXTENDED:
        {
            switch (effect.data.ext.cmd)
            {
                case PT_ECMD_FILTER:                    // E0x
                {
                    effect.data.ext.value = (effect.data.ext.value & 1) << 1;
                }
                break;

                case PT_ECMD_CUT_SAMPLE:                // ECx
                {
                    if (effect.data.ext.value == 0)
                    {
                        effect.cmd = PT_CMD_SET_VOLUME;
                        effect.data.value = 0;
                    }
                }
                break;

                case PT_ECMD_FINESLIDE_UP:              // E1x
                case PT_ECMD_FINESLIDE_DOWN:            // E2x
                case PT_ECMD_RETRIGGER_SAMPLE:          // E9x
                case PT_ECMD_FINE_VOLUME_SLIDE_UP:      // EAx
                case PT_ECMD_FINE_VOLUME_SLIDE_DOWN:    // EBx
                case PT_ECMD_DELAY_SAMPLE:              // EDx
                case PT_ECMD_DELAY_PATTERN:             // EEx
                {
                    has_command = effect.data.ext.value != 0;
                }
                break;
            }
        }
        break;
    }

    if (!has_command)
    {
        effect.cmd = 0;
        effect.data.value = 0;
    }

    *usecode |= (effect.cmd == PT_CMD_EXTENDED) ? (1 << (effect.data.ext.cmd + 16)) : (1 << (effect.cmd));

    // empty channel
    if (!note && !instrument && !has_command)
    {
        // o1111111
        out->data[0] = CHANNEL_EMPTY;
        out->data[1] = out->data[2] = 0;
        return 1;
    }

    // note + instrument
    if (note && instrument && !has_command)
    {
        // o1110nnn nnniiiii
        out->data[0] = CHANNEL_NOTE_INSTRUMENT | ((note >> 3) & 0x7);
        out->data[1] = ((note << 5) & 0xe0) | (instrument & 0x1f);
        out->data[2] = 0;
        return 2;
    }

    // command only
    if (!note && !instrument && has_command)
    {
        // o110cccc bbbbbbbb
        out->data[0] = CHANNEL_COMMAND | (effect.cmd & 0x0f);
        out->data[1] = effect.data.value;
        out->data[2] = 0;
        return 2;
    }

    // note + instrument + command
    // onnnnnni iiiicccc bbbbbbbb

    out->data[0] = CHANNEL_ALL | ((note << 1) & 0x7e) | ((instrument >> 4) & 0x01);
    out->data[1] = ((instrument << 4) & 0xf0) | (effect.cmd & 0x0f);
    out->data[2] = effect.data.value;
    return 3;
}

static size_t build_channel_entries(player61a_channel_t* channel, const protracker_pattern_t* pattern, size_t channel_index, uint32_t* usecode)
{
    for (size_t i = 0; i < PT_PATTERN_ROWS; ++i)
    {
        const protracker_pattern_row_t* row = &(pattern->rows[i]);
        const protracker_channel_t* in = &(row->channels[channel_index]);

        player61a_channel_t out;
        size_t size = convert_channel(&out, in, row, channel_index, usecode);
    }
    return PT_PATTERN_ROWS;
}

static void build_patterns(player61a_t* output, const protracker_t* input, const char* options, uint32_t* usecode)
{
    LOG_DEBUG("Converting patterns...\n");

    output->header.num_patterns = input->num_patterns;

    output->song.length = input->song.length;
    memcpy(output->song.positions, input->song.positions, sizeof(uint8_t) * PT_NUM_POSITIONS);

    output->pattern_offsets = malloc(input->num_patterns * sizeof(player61a_pattern_offset_t));
    memset(output->pattern_offsets, 0, input->num_patterns * sizeof(player61a_pattern_offset_t));

    for (size_t i = 0; i < input->num_patterns; ++i)
    {
        for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
        {
            player61a_channel_t channel[PT_PATTERN_ROWS];
            size_t length = build_channel_entries(channel, &(input->patterns[i]), j, usecode);

            LOG_TRACE("%lu %lu - %lu\n", i, j, length);
        }
    }
}

static void player61a_create(player61a_t* module)
{
    memset(module, 0, sizeof(player61a_t));

    buffer_init(&(module->patterns), 1);
    buffer_init(&(module->samples), 1);
}

static void player61a_destroy(player61a_t* module)
{
    free(module->pattern_offsets);

    buffer_release(&(module->patterns));
    buffer_release(&(module->samples));
}

#if 0

static int8_t deltas[] = {
    0,1,2,4,8,16,32,64,128,-64,-32,-16,-8,-4,-2,-1
};

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
    size_t size = buffer_count(&(module->samples));
    if (size > 0)
    {
        buffer_add(buffer, buffer_get(&(module->samples), 0), size);
    }
}

bool player61a_convert(buffer_t* buffer, const protracker_t* module, const char* options)
{
    LOG_INFO("Converting to The Player 6.1A...\n");

    player61a_t temp;
    player61a_create(&temp);
    uint32_t usecode = 0;

    build_samples(&temp, module, options);
    build_patterns(&temp, module, options, &usecode);

    LOG_TRACE("usecode: %08x\n", usecode);

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
