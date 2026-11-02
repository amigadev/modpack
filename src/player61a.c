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

static const int8_t deltas[] = {
    0,1,2,4,8,16,32,64,-128,-64,-32,-16,-8,-4,-2,-1
};

static void pack_sample(buffer_t* buffer, const uint8_t* data, size_t length)
{
    uint8_t* out = buffer_alloc(buffer, length / 2);
    uint8_t value = 0;

    for (size_t i = 0; i < length; ++i)
    {
        uint8_t index = 0;
        int best = 256;

        for (uint8_t j = 0; j < 16; ++j)
        {
            int diff = abs((int8_t)(value - (uint8_t)deltas[j]) - (int8_t)data[i]);
            if (diff < best)
            {
                index = j;
                best = diff;
            }
        }

        value -= (uint8_t)deltas[index];

        if (i & 1)
        {
            out[i / 2] |= index;
        }
        else
        {
            out[i / 2] = index << 4;
        }
    }
}

static void delta_sample(buffer_t* buffer, const uint8_t* data, size_t length)
{
    uint8_t* out = buffer_alloc(buffer, length);

    out[0] = data[0];
    for (size_t i = 1; i < length; ++i)
    {
        out[i] = data[i - 1] - data[i];
    }
}

static bool build_samples(player61a_t* output, const protracker_t* module, const char* options, uint32_t* usecode)
{
    LOG_DEBUG("Building sample table:\n");

    bool pack[PT_NUM_SAMPLES];
    if (!get_option_range(options, "4bit", pack, PT_NUM_SAMPLES))
    {
        return false;
    }
    bool delta = has_option(options, "delta", false);
    bool packed_samples = false;

    bool usage[PT_NUM_SAMPLES];
    protracker_get_used_samples(module, usage);

    size_t sources[PT_NUM_SAMPLES];
    size_t sample_count = 0;
    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        const protracker_sample_t* input = &(module->sample_headers[i]);

        if (!usage[i])
        {
            output->sample_map[i] = 0;
            continue;
        }

        sources[sample_count] = i;
        p61a_sample_t* sample = &(output->sample_headers[sample_count++]);
        output->sample_map[i] = (uint8_t)sample_count;

        uint16_t length = 0;
        bool looped = input->repeat_length > 1;
        if (looped && input->length && (input->repeat_offset >= input->length))
        {
            LOG_WARN("Sample #%lu loop starts beyond sample end, loop removed.\n", (i+1));
            looped = false;
        }

        if (!input->length)
        {
            // empty

            sample->length = 1;
            sample->finetone = 0;
            sample->volume = 0;
            sample->repeat_offset = 0xffff;
        }
        else if (looped)
        {
            // looping

            uint32_t loop_end = (uint32_t)input->repeat_offset + input->repeat_length;
            length = loop_end > input->length ? input->length : (uint16_t)loop_end;
            LOG_TRACE(" #%lu - %u bytes (looped)\n", (i+1), length * 2);

            if (loop_end > input->length)
            {
                LOG_WARN("Looped sample #%lu loop ends beyond sample end, loop shortened.\n", (i+1));
            }
            else if (length != input->length)
            {
                LOG_WARN("Looped sample #%lu truncated (%u -> %u bytes).\n", (i+1), input->length * 2, length * 2);
            }

            sample->length = length;
            sample->finetone = input->finetone & 0x0f;
            sample->volume = input->volume > 64 ? 64 : input->volume;
            sample->repeat_offset = input->repeat_offset;
        }
        else
        {
            // not looping

            const uint8_t* data = module->sample_data[i];

            length = input->length;
            while ((length > 1) && !data[length * 2 - 2] && !data[length * 2 - 1])
            {
                --length;
            }
            LOG_TRACE(" #%lu - %u bytes\n", (i+1), length * 2);

            if (length != input->length)
            {
                LOG_DEBUG("Sample #%lu trimmed (%u -> %u bytes).\n", (i+1), input->length * 2, length * 2);
            }

            sample->length = length;
            sample->finetone = input->finetone & 0x0f;
            sample->volume = input->volume > 64 ? 64 : input->volume;
            sample->repeat_offset = 0xffff;
        }

        if (sample->finetone)
        {
            *usecode |= 1; // mark finetones used in usecode
        }

        bool shared = false;
        for (size_t k = 0; (length > 1) && (k + 1 < sample_count); ++k)
        {
            const p61a_sample_t* other = &(output->sample_headers[k]);
            if ((other->length == length) && (((other->finetone & 0x80) != 0) == pack[i]) && !memcmp(module->sample_data[sources[k]], module->sample_data[i], length * 2))
            {
                LOG_DEBUG("Sample #%lu shares data with sample #%lu.\n", (i+1), (sources[k]+1));
                sample->length = (uint16_t)(0xffff - k);
                shared = true;
                break;
            }
        }

        if (shared)
        {
            continue;
        }

        output->sample_buffer_size += sample->length * 2;

        if (input->length > 0)
        {
            if (pack[i] && length > 1)
            {
                LOG_TRACE(" #%lu - 4-bit packed\n", (i+1));
                sample->finetone |= 0x80;
                packed_samples = true;
                pack_sample(&(output->samples), module->sample_data[i], length * 2);
            }
            else if (delta)
            {
                delta_sample(&(output->samples), module->sample_data[i], length * 2);
            }
            else
            {
                buffer_add(&(output->samples), module->sample_data[i], length * 2);
            }
        }
        else
        {
            uint16_t temp = 0;
            buffer_add(&(output->samples), &temp, sizeof(temp));
        }
    }

    LOG_DEBUG(" %lu samples used.\n", sample_count);

    output->header.sample_count = (uint8_t)sample_count | (packed_samples ? 0x40 : 0) | (delta ? 0x80 : 0);
    return true;
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
#define CHANNEL_COMPRESSED      (0x80)

#define COMPRESSION_CMD_BITS    (0xc0)
#define COMPRESSION_DATA_BITS   (0x3f)

#define COMPRESSION_EMPTY_ROWS  (0x00)  // Next N rows are empty
#define COMPRESSION_REPEAT_ROWS (0x80)  // Repeat this row N times
#define COMPRESSION_JUMP        (0x40)  // 8-bit jump
#define COMPRESSION_JUMP_LONG   (0x80)  // 16-bit jump (requires COMPRESSION_JUMP)

static uint16_t periods[] = {
    856,808,762,720,678,640,604,570,538,508,480,453,    // octave 1
    428,404,381,360,339,320,302,285,269,254,240,226,    // octave 2
    214,202,190,180,170,160,151,143,135,127,120,113     // octave 3
};

static uint8_t index_from_period(uint16_t period)
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

static uint16_t period_from_index(uint8_t index)
{
    if (!index || index > (sizeof(periods) / sizeof(uint16_t)))
    {
        return 0;
    }

    return periods[index-1];
}

typedef struct
{
    size_t patterns;
    size_t samples;
} p61a_offsets_t;

static size_t get_sample_offset(const player61a_t* module)
{
    size_t curr = 0;

    curr += sizeof(p61a_header_t);   // header
    if (module->header.sample_count & 0x40)
    {
        curr += sizeof(uint32_t); // sample buffer size
    }
    curr += sizeof(p61a_sample_t) * (module->header.sample_count & 0x1f); // sample headers
    curr += sizeof(p61a_pattern_offset_t) * module->header.pattern_count; // pattern offsets
    curr += module->song.length+1; // song positions (+0xff)

    curr += buffer_count(&(module->patterns)); // tracks

    if (curr & 1)
    {
        ++curr;
    }

    return curr;
}

static size_t get_channel_length(const p61a_channel_t* channel)
{
    if ((channel->data[0] & CHANNEL_EMPTY) == CHANNEL_EMPTY)
        return 1;

    if ((channel->data[0] & CHANNEL_NOTE_INSTRUMENT) == CHANNEL_NOTE_INSTRUMENT)
        return 2;

    if ((channel->data[0] & CHANNEL_COMMAND) == CHANNEL_COMMAND)
        return 2;

    return 3;
}

static bool to_signed_slide(protracker_effect_t* effect)
{
    uint8_t up = effect->data.value >> 4;
    uint8_t down = effect->data.value & 0x0f;

    if (up)
    {
        effect->data.value = (uint8_t)(-up);
        return true;
    }

    effect->data.value = down;
    return down != 0;
}

static size_t to_p61a_channel(p61a_channel_t* out, const protracker_channel_t* in, const protracker_pattern_row_t* row, size_t channel_index, const uint8_t* sample_map, uint32_t* usecode)
{
    uint8_t instrument = protracker_get_sample(in);
    instrument = instrument ? sample_map[instrument - 1] : 0;
    uint16_t period = protracker_get_period(in);
    protracker_effect_t effect = protracker_get_effect(in);

    uint8_t note = index_from_period(period);

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

        case PT_CMD_CONTINUE_SLIDE:     // 500 -> 300, 5xy -> signed slide
        {
            if (!effect.data.value)
            {
                effect.cmd = PT_CMD_SLIDE_TO_NOTE;
            }
            else
            {
                has_command = to_signed_slide(&effect);
            }
        }
        break;

        case PT_CMD_CONTINUE_VIBRATO:   // 600 -> 400, 6xy -> signed slide
        {
            if (!effect.data.value)
            {
                effect.cmd = PT_CMD_VIBRATO;
            }
            else
            {
                has_command = to_signed_slide(&effect);
            }
        }
        break;

        case PT_CMD_VOLUME_SLIDE:       // Axy -> signed slide, A00 removed
        {
            has_command = to_signed_slide(&effect);
        }
        break;

        case PT_CMD_SET_VOLUME:
        {
            effect.data.value = effect.data.value > 64 ? 64 : effect.data.value;
        }
        break;

        case PT_CMD_8:  // 8xy -> E8y
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

    if (has_command)
    {
        *usecode |= (effect.cmd == PT_CMD_EXTENDED) ? (1 << (effect.data.ext.cmd + 16)) : (1 << (effect.cmd));
    }
    else
    {
        effect.cmd = 0;
        effect.data.value = 0;
    }

    // empty channel
    if (!note && !instrument && !has_command)
    {
        // o1111111
        out->data[0] = CHANNEL_EMPTY;
        out->data[1] = out->data[2] = 0;
        return 1;
    }

    // note + instrument
    if ((note || instrument) && !has_command)
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

static bool to_protracker_channel(protracker_channel_t* out, const p61a_channel_t* in)
{
    memset(out, 0, sizeof(protracker_channel_t));

    if ((in->data[0] & CHANNEL_EMPTY) == CHANNEL_EMPTY)
    {
        // CHANNEL_EMPTY
    }
    else if ((in->data[0] & CHANNEL_NOTE_INSTRUMENT) == CHANNEL_NOTE_INSTRUMENT)
    {
        // CHANNEL_NOTE_INSTRUMENT - o1110nnn nnniiiii
        uint8_t note = ((in->data[0] & 0x07) << 3) | ((in->data[1] & 0xe0) >> 5);
        uint8_t sample = in->data[1] & 0x1f;

        protracker_set_period(out, period_from_index(note));
        protracker_set_sample(out, sample);
    }
    else if ((in->data[0] & CHANNEL_COMMAND) == CHANNEL_COMMAND)
    {
        // CHANNEL_COMMAND - o110cccc bbbbbbbb
        protracker_effect_t effect;
        effect.cmd = in->data[0] & 0x0f;
        effect.data.value = in->data[1];

        protracker_set_effect(out, &effect);
    }
    else
    {
        // CHANNEL_ALL - onnnnnni iiiicccc bbbbbbbb
        uint8_t note = (in->data[0] & 0x7e) >> 1;
        uint8_t sample = ((in->data[0] & 0x01) << 4) | ((in->data[1] & 0xf0) >> 4);

        protracker_effect_t effect;
        effect.cmd = in->data[1] & 0x0f;
        effect.data.value = in->data[2];

        protracker_set_period(out, period_from_index(note));
        protracker_set_sample(out, sample);
        protracker_set_effect(out, &effect);
    }

    {
        protracker_effect_t effect = protracker_get_effect(out);

        switch (effect.cmd)
        {
            case PT_CMD_8:
            {
                effect.cmd = PT_CMD_ARPEGGIO;
                protracker_set_effect(out, &effect);
            }
            break;

            case PT_CMD_CONTINUE_SLIDE:
            case PT_CMD_CONTINUE_VIBRATO:
            case PT_CMD_VOLUME_SLIDE:
            {
                if (effect.data.value & 0x80)
                {
                    effect.data.value = (uint8_t)((-effect.data.value) & 0x0f) << 4;
                    protracker_set_effect(out, &effect);
                }
            }
            break;

            case PT_CMD_EXTENDED:
            {
                if (effect.data.ext.cmd == PT_ECMD_FILTER)
                {
                    effect.data.ext.value >>= 1;
                    protracker_set_effect(out, &effect);
                }
            }
            break;
        }
    }

    return true;
}

static size_t build_track(p61a_channel_t* channel, const protracker_pattern_t* pattern, size_t channel_index, const uint8_t* sample_map, uint32_t* usecode)
{
    for (size_t i = 0; i < PT_PATTERN_ROWS; ++i)
    {
        const protracker_pattern_row_t* row = &(pattern->rows[i]);
        protracker_channel_t in = row->channels[channel_index];

        size_t break_channel = PT_NUM_CHANNELS;
        for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
        {
            protracker_effect_t effect = protracker_get_effect(&(row->channels[j]));
            if ((effect.cmd == PT_CMD_POS_JUMP) || ((effect.cmd == PT_CMD_PATTERN_BREAK) && (i < PT_PATTERN_ROWS - 1)))
            {
                break_channel = j;
                break;
            }
        }

        protracker_effect_t effect = protracker_get_effect(&in);
        if (((effect.cmd == PT_CMD_POS_JUMP) || (effect.cmd == PT_CMD_PATTERN_BREAK)) && (channel_index != break_channel))
        {
            memset(&effect, 0, sizeof(effect));
            protracker_set_effect(&in, &effect);
        }

        to_p61a_channel(&(channel[i]), &in, row, channel_index, sample_map, usecode);

        if (break_channel < PT_NUM_CHANNELS)
        {
            return i + 1;
        }
    }
    return PT_PATTERN_ROWS;
}

typedef struct
{
    uint8_t data[P61A_CHANNEL_BYTES + 1];
    size_t length;
} p61a_entry_t;

static size_t build_entries(p61a_entry_t* entries, const p61a_channel_t* track, size_t length, bool compress)
{
    size_t count = 0;
    const p61a_channel_t* last = NULL;

    for (size_t i = 0; i < length; ++i)
    {
        const p61a_channel_t* channel = &(track[i]);

        if (compress && last)
        {
            p61a_entry_t* entry = &(entries[count - 1]);
            bool empty = channel->data[0] == CHANNEL_EMPTY;
            bool same = !memcmp(channel->data, last->data, P61A_CHANNEL_BYTES);

            if (!(entry->data[0] & CHANNEL_COMPRESSED))
            {
                if (empty || same)
                {
                    entry->data[0] |= CHANNEL_COMPRESSED;
                    entry->data[entry->length++] = (empty ? COMPRESSION_EMPTY_ROWS : COMPRESSION_REPEAT_ROWS) | 1;
                    continue;
                }
            }
            else
            {
                uint8_t* compression = &(entry->data[entry->length - 1]);
                if ((empty && ((*compression & COMPRESSION_CMD_BITS) == COMPRESSION_EMPTY_ROWS)) ||
                    (same && ((*compression & COMPRESSION_CMD_BITS) == COMPRESSION_REPEAT_ROWS)))
                {
                    ++(*compression);
                    continue;
                }
            }
        }

        p61a_entry_t* entry = &(entries[count++]);
        entry->length = get_channel_length(channel);
        memcpy(entry->data, channel->data, entry->length);
        last = channel;
    }

    return count;
}

static size_t get_entry_length(const uint8_t* data)
{
    size_t length = get_channel_length((const p61a_channel_t*)data);

    if (data[0] & CHANNEL_COMPRESSED)
    {
        uint8_t compression = data[length++];

        if (((data[0] & CHANNEL_EMPTY) == CHANNEL_EMPTY) && (compression & COMPRESSION_JUMP))
        {
            length += (compression & COMPRESSION_JUMP_LONG) ? 2 : 1;
        }
    }

    return length;
}

static bool is_same_entry(const uint8_t* data, const p61a_entry_t* entry)
{
    return (get_entry_length(data) == entry->length) && !memcmp(data, entry->data, entry->length);
}

static size_t find_jump(const buffer_t* buffer, const p61a_entry_t* entries, size_t index, size_t end, size_t* target)
{
    size_t size = buffer_count(buffer);
    if (!size)
    {
        return 0;
    }

    const uint8_t* data = buffer_get(buffer, 0);
    size_t best_count = 0;
    ptrdiff_t best_gain = 0;

    size_t start = 0;
    while (start < size)
    {
        size_t next = start + get_entry_length(data + start);

        if (!is_same_entry(data + start, &(entries[index])))
        {
            start = next;
            continue;
        }

        if ((index + 1 >= end) || (next >= size))
        {
            break;
        }

        size_t curr = next;
        size_t i = index + 1;
        size_t count = 0;
        do
        {
            bool same = is_same_entry(data + curr, &(entries[i]));
            curr += get_entry_length(data + curr);

            if (!same)
            {
                break;
            }

            ++i;
            ++count;
        }
        while ((curr < size) && (i < end));

        if (count)
        {
            ptrdiff_t gain = (ptrdiff_t)(curr - start) - 3;
            if (size - start >= 256)
            {
                --gain;
            }

            if ((gain > best_gain) && (size + 4 - start <= 0xffff))
            {
                best_gain = gain;
                best_count = count + 1;
                *target = start;
            }
        }

        start = next;
    }

    return best_count;
}

static void write_jump(buffer_t* buffer, size_t target, size_t count)
{
    uint8_t data[4];
    size_t length = 0;
    size_t distance = buffer_count(buffer) + 3 - target;

    data[length++] = CHANNEL_EMPTY | CHANNEL_COMPRESSED;
    if (distance < 256)
    {
        data[length++] = COMPRESSION_JUMP | (uint8_t)(count - 1);
        data[length++] = (uint8_t)distance;
    }
    else
    {
        ++distance;
        data[length++] = COMPRESSION_JUMP | COMPRESSION_JUMP_LONG | (uint8_t)(count - 1);
        data[length++] = (uint8_t)(distance >> 8);
        data[length++] = (uint8_t)distance;
    }

    buffer_add(buffer, data, length);
}

static bool build_patterns(player61a_t* output, const protracker_t* input, const char* options, uint32_t* usecode)
{
    LOG_DEBUG("Converting patterns...\n");

    bool compress = has_option(options, "compress_patterns", true);

    output->header.pattern_count = input->num_patterns;

    output->song.length = input->song.length;
    memcpy(output->song.positions, input->song.positions, sizeof(uint8_t) * PT_NUM_POSITIONS);

    output->pattern_offsets = malloc(input->num_patterns * sizeof(p61a_pattern_offset_t));
    memset(output->pattern_offsets, 0, input->num_patterns * sizeof(p61a_pattern_offset_t));

    size_t track_count = input->num_patterns * PT_NUM_CHANNELS;
    p61a_entry_t* entries = malloc(track_count * PT_PATTERN_ROWS * sizeof(p61a_entry_t));
    size_t* tracks = malloc((track_count + 1) * sizeof(size_t));
    size_t entry_count = 0;

    for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
    {
        for (size_t i = 0; i < input->num_patterns; ++i)
        {
            p61a_channel_t track[PT_PATTERN_ROWS];

            size_t length = build_track(track, &(input->patterns[i]), j, output->sample_map, usecode);

            tracks[j * input->num_patterns + i] = entry_count;
            entry_count += build_entries(&(entries[entry_count]), track, length, compress);
        }
    }
    tracks[track_count] = entry_count;

    bool success = true;
    for (size_t k = 0; k < track_count; ++k)
    {
        size_t i = k % input->num_patterns;
        size_t j = k / input->num_patterns;

        size_t offset = buffer_count(&(output->patterns));
        if (offset > 0xffff)
        {
            LOG_ERROR("Pattern data too large, track offsets must fit in 16 bits (pattern %lu, channel %lu).\n", i, j);
            success = false;
            break;
        }

        output->pattern_offsets[i].channels[j] = offset;

        size_t index = tracks[k];
        while (index < tracks[k + 1])
        {
            size_t target;
            size_t count = compress ? find_jump(&(output->patterns), entries, index, tracks[k + 1], &target) : 0;

            if (count)
            {
                write_jump(&(output->patterns), target, count);
                index += count;
            }
            else
            {
                buffer_add(&(output->patterns), entries[index].data, entries[index].length);
                ++index;
            }
        }
    }

    LOG_DEBUG(" %lu bytes of pattern data.\n", buffer_count(&(output->patterns)));

    free(tracks);
    free(entries);

    return success;
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

static void write_song(buffer_t* buffer, const player61a_t* module, const char* options)
{
    if (has_option(options, "sign", false))
    {
        LOG_TRACE(" - Adding signature.\n");
        buffer_add(buffer, signature, strlen(signature));
    }

    // header

    {
        p61a_header_t header;

        header.sample_offset = htons(get_sample_offset(module));
        header.pattern_count = module->header.pattern_count;
        header.sample_count = module->header.sample_count;

        buffer_add(buffer, &header, sizeof(header));
    }

    if (module->header.sample_count & 0x40)
    {
        uint32_t size = htonl(module->sample_buffer_size);
        buffer_add(buffer, &size, sizeof(size));
    }

    // sample headers

    for (size_t i = 0; i < (module->header.sample_count & 0x1f); ++i)
    {
        const p61a_sample_t* in = &(module->sample_headers[i]);
        p61a_sample_t sample;

        bool empty = in->length == 0;

        if (!empty)
        {
            sample.length = htons(in->length);
            sample.finetone = in->finetone;
            sample.volume = in->volume;
            sample.repeat_offset = htons(in->repeat_offset);
        }
        else
        {
            sample.length = htons(1);
            sample.finetone = 0;
            sample.volume = 0;
            sample.repeat_offset = htons(0xffff);
        }

        buffer_add(buffer, &sample, sizeof(sample));
    }

    // pattern offsets

    for (size_t i = 0; i < module->header.pattern_count; ++i)
    {
        p61a_pattern_offset_t offset;
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

    // tracks

    size_t pattern_size = buffer_count(&(module->patterns));
    if (pattern_size)
    {
        buffer_add(buffer, buffer_get(&(module->patterns), 0), pattern_size);
    }

    // align samples

    if (buffer_count(buffer) & 1)
    {
        uint8_t c = 0;
        buffer_add(buffer, &c, 1);
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

    if (!build_samples(&temp, module, options, &usecode) || !build_patterns(&temp, module, options, &usecode))
    {
        player61a_destroy(&temp);
        return false;
    }

    if (get_sample_offset(&temp) > 0xffff)
    {
        LOG_ERROR("Song data too large, sample offset must fit in 16 bits (%lu bytes).\n", get_sample_offset(&temp));
        player61a_destroy(&temp);
        return false;
    }

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

static const uint8_t* read_sample_headers(p61a_sample_t* sample_headers, size_t sample_count, const uint8_t* curr, const uint8_t* max)
{
    LOG_TRACE("Samples:\n");
    for (size_t i = 0; i < sample_count; ++i)
    {
        if ((max - curr) < sizeof(p61a_sample_t))
        {
            LOG_ERROR("Premature end of data before sample #%lu.\n", (i+1));
            return NULL;
        }

        p61a_sample_t sample;
        memcpy(&sample, curr, sizeof(sample));
        curr += sizeof(sample);

        sample.length = ntohs(sample.length);
        sample.finetone = sample.finetone;
        sample.volume = sample.volume;
        sample.repeat_offset = ntohs(sample.repeat_offset);

        LOG_TRACE(" #%02u - length: $%04X, finetone: %u, volume: %u, repeat offset: $%04X\n",
            (i+1),
            sample.length,
            sample.finetone,
            sample.volume,
            sample.repeat_offset
        );

        sample_headers[i] = sample;
    }

    return curr;
}

static const uint8_t* read_pattern_offsets(p61a_pattern_offset_t* pattern_offsets, size_t pattern_count, const uint8_t* curr, const uint8_t* max)
{
    LOG_TRACE("Pattern Offsets:\n");
    for (size_t i = 0; i < pattern_count; ++i)
    {
        if ((max - curr) < sizeof(p61a_pattern_offset_t))
        {
            LOG_ERROR("Premature end of data before pattern offset %lu.\n", i);
            return NULL;
        }

        p61a_pattern_offset_t offset;
        memcpy(&offset, curr, sizeof(offset));
        curr += sizeof(offset);

        LOG_TRACE(" #%lu:", i);
        for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
        {
            offset.channels[j] = ntohs(offset.channels[j]);
            LOG_TRACE(" %04X", offset.channels[j]);
        }
        LOG_TRACE("\n");

        pattern_offsets[i] = offset;
    }
    return curr;
}

static const uint8_t* read_song_positions(p61a_song_t* song, size_t pattern_count, const uint8_t* curr, const uint8_t* max)
{
    LOG_TRACE("Song Positions:\n ");
    for (size_t i = 0; i <= PT_NUM_POSITIONS; ++i)
    {
        if ((max - curr) < sizeof(uint8_t))
        {
            LOG_ERROR("Premature end of data before song position %lu.\n", i);
            return NULL;
        }

        uint8_t position = *curr++;
        if (position == 0xff)
        {
            song->length = i;
            LOG_TRACE("\n");
            return curr;
        }

        if (i == PT_NUM_POSITIONS)
        {
            break;
        }

        if (position >= pattern_count)
        {
            LOG_ERROR("Invalid pattern %u at song position %lu.\n", position, i);
            return NULL;
        }

        LOG_TRACE(" %u", position);

        song->positions[i] = position;
    }

    LOG_ERROR("Song positions not terminated.\n");
    return NULL;
}

static int read_entry(p61a_channel_t* out, const uint8_t** curr, const uint8_t* base, const uint8_t* max)
{
    const uint8_t* src = *curr;

    if (src < base || src >= max)
    {
        return -2;
    }

    memset(out, 0, sizeof(p61a_channel_t));
    out->data[0] = src[0];

    size_t length = get_channel_length(out);
    if ((size_t)(max - src) < length + ((src[0] & CHANNEL_COMPRESSED) ? 1 : 0))
    {
        return -2;
    }

    memcpy(out->data, src, length);
    out->data[0] &= ~CHANNEL_COMPRESSED;
    src += length;

    int info = -1;
    if ((*curr)[0] & CHANNEL_COMPRESSED)
    {
        info = *src++;
    }

    *curr = src;
    return info;
}

typedef struct
{
    const uint8_t* curr;
    const uint8_t* jump;
    size_t jump_left;
    size_t repeat;
    bool repeat_same;
    p61a_channel_t last;
} p61a_track_reader_t;

static bool read_row(p61a_track_reader_t* reader, p61a_channel_t* out, const uint8_t* base, const uint8_t* max)
{
    if (reader->repeat)
    {
        --reader->repeat;

        if (reader->repeat_same)
        {
            *out = reader->last;
        }
        else
        {
            memset(out, 0, sizeof(p61a_channel_t));
            out->data[0] = CHANNEL_EMPTY;
        }
        return true;
    }

    int info;
    if (reader->jump_left)
    {
        --reader->jump_left;
        info = read_entry(out, &(reader->jump), base, max);
    }
    else
    {
        info = read_entry(out, &(reader->curr), base, max);

        if (info >= 0 && (info & COMPRESSION_JUMP))
        {
            const uint8_t* src = reader->curr;
            size_t bytes = (info & COMPRESSION_JUMP_LONG) ? 2 : 1;
            if ((size_t)(max - src) < bytes)
            {
                LOG_ERROR("Premature end of pattern data.\n");
                return false;
            }

            size_t dist = (bytes == 2) ? ((src[0] << 8) | src[1]) : src[0];
            reader->curr += bytes;

            if (dist > (size_t)(reader->curr - base))
            {
                LOG_ERROR("Invalid back-reference in pattern data.\n");
                return false;
            }

            reader->jump = reader->curr - dist;
            reader->jump_left = info & COMPRESSION_DATA_BITS;

            info = read_entry(out, &(reader->jump), base, max);
            if (info >= 0 && (info & COMPRESSION_JUMP))
            {
                LOG_ERROR("Nested back-reference in pattern data.\n");
                return false;
            }
        }
    }

    if (info == -2)
    {
        LOG_ERROR("Premature end of pattern data.\n");
        return false;
    }

    if (info >= 0)
    {
        reader->repeat = info & COMPRESSION_DATA_BITS;
        reader->repeat_same = (info & COMPRESSION_REPEAT_ROWS) != 0;
    }

    reader->last = *out;
    return true;
}

static uint8_t get_command(const p61a_channel_t* channel)
{
    uint8_t c0 = channel->data[0];

    if ((c0 & CHANNEL_EMPTY) == CHANNEL_EMPTY || (c0 & CHANNEL_NOTE_INSTRUMENT) == CHANNEL_NOTE_INSTRUMENT)
        return 0;

    if ((c0 & CHANNEL_COMMAND) == CHANNEL_COMMAND)
        return c0 & 0x0f;

    return channel->data[1] & 0x0f;
}

static bool read_patterns(p61a_pattern_t* patterns, p61a_pattern_offset_t* pattern_offsets, size_t pattern_count, const uint8_t* curr, const uint8_t* max)
{
    for (size_t i = 0; i < pattern_count; ++i)
    {
        p61a_pattern_t* pattern = &(patterns[i]);
        const p61a_pattern_offset_t* offsets = &(pattern_offsets[i]);

        p61a_track_reader_t readers[PT_NUM_CHANNELS];
        memset(readers, 0, sizeof(readers));

        for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
        {
            readers[j].curr = curr + offsets->channels[j];
        }

        for (size_t row = 0; row < PT_PATTERN_ROWS; ++row)
        {
            bool brk = false;

            for (size_t j = 0; j < PT_NUM_CHANNELS; ++j)
            {
                p61a_channel_t* out = &(pattern->rows[row].channels[j]);

                if (!read_row(&(readers[j]), out, curr, max))
                {
                    LOG_ERROR("Failed to decode pattern %lu, track %lu, row %lu.\n", i, j, row);
                    return false;
                }

                uint8_t cmd = get_command(out);
                brk = brk || cmd == PT_CMD_POS_JUMP || cmd == PT_CMD_PATTERN_BREAK;
            }

            if (brk)
            {
                break;
            }
        }
    }

    return true;
}

protracker_t* player61a_load(const buffer_t* buffer)
{
    LOG_DEBUG("Loading Player 6.1A module...\n");

    p61a_pattern_t* patterns = NULL;

    protracker_t module;
    protracker_create(&module);

    size_t signature_length = strlen(signature);

    do
    {
        size_t size = buffer_count(buffer);
        const uint8_t* raw = buffer_get(buffer, 0);

        // check for P61A (and skip it)
        if (size >= signature_length && !memcmp(signature, raw, signature_length))
        {
            raw += signature_length;
            size -= signature_length;
        }

        if (size < sizeof(p61a_header_t))
        {
            LOG_ERROR("Premature end of data before header.\n");
            break;
        }

        const uint8_t* curr = raw;
        const uint8_t* max = curr + size;

        // header

        p61a_header_t header;
        memcpy(&header, curr, sizeof(header));
        curr += sizeof(p61a_header_t);

        header.sample_offset = ntohs(header.sample_offset);
        uint8_t sample_count = header.sample_count & 0x1f;
        bool packed_samples = (header.sample_count & 0x40) != 0;
        bool delta_samples = (header.sample_count & 0x80) != 0;
        LOG_TRACE("Header:\n Sample Offset: %u\n Patterns:%u\n Sample count:%u%s%s\n", header.sample_offset, header.pattern_count, sample_count, packed_samples ? " (4-bit packed)" : "", delta_samples ? " (delta)" : "");

        if (!header.pattern_count || header.pattern_count > PT_NUM_POSITIONS)
        {
            LOG_ERROR("Invalid pattern count in header. (%u)\n", header.pattern_count);
            break;
        }

        if (header.sample_offset > size)
        {
            LOG_ERROR("Invalid sample offset in header. (%u)\n", header.sample_offset);
            break;
        }

        if (packed_samples)
        {
            if ((max - curr) < sizeof(uint32_t))
            {
                LOG_ERROR("Premature end of data before sample buffer size.\n");
                break;
            }
            curr += sizeof(uint32_t);
        }

        // sample headers

        p61a_sample_t sample_headers[PT_NUM_SAMPLES];
        if (!(curr = read_sample_headers(sample_headers, sample_count, curr, max)))
        {
            break;
        }

        // pattern offsets

        p61a_pattern_offset_t pattern_offsets[header.pattern_count];
        if (!(curr = read_pattern_offsets(pattern_offsets, header.pattern_count, curr, max)))
        {
            break;
        }

        // song positions

        p61a_song_t song = { 0 };
        if (!(curr = read_song_positions(&song, header.pattern_count, curr, max)))
        {
            break;
        }

        // patterns

        const uint8_t* pattern_end = raw + header.sample_offset;
        if (pattern_end < curr)
        {
            LOG_ERROR("Invalid sample offset in header. (%u)\n", header.sample_offset);
            break;
        }

        patterns = malloc(sizeof(p61a_pattern_t) * header.pattern_count);
        memset(patterns, 0, sizeof(p61a_pattern_t) * header.pattern_count);
        for (size_t i = 0; i < header.pattern_count; ++i)
        {
            for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
            {
                for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
                {
                    patterns[i].rows[j].channels[k].data[0] = CHANNEL_EMPTY;
                }
            }
        }

        if (!read_patterns(patterns, pattern_offsets, header.pattern_count, curr, pattern_end))
        {
            break;
        }

        // PT: header

        bool valid = true;
        for (size_t i = 0; i < sample_count; ++i)
        {
            const p61a_sample_t* in = &(sample_headers[i]);
            protracker_sample_t* out = &(module.sample_headers[i]);

            uint16_t length = in->length;
            if (length & 0x8000)
            {
                size_t source = (size_t)(0x10000 - length) - 1;
                if (source >= i || (sample_headers[source].length & 0x8000))
                {
                    LOG_ERROR("Invalid shared sample reference in sample #%lu.\n", (i+1));
                    valid = false;
                    break;
                }
                length = sample_headers[source].length;
            }

            out->length = length;
            out->finetone = in->finetone & 0x0f;
            out->volume = in->volume;

            if (in->repeat_offset == 0xffff || in->repeat_offset >= length)
            {
                out->repeat_offset = 0;
                out->repeat_length = 1;
            }
            else
            {
                out->repeat_offset = in->repeat_offset;
                out->repeat_length = length - in->repeat_offset;
            }
        }

        if (!valid)
        {
            break;
        }

        // PT: Song

        module.song.length = song.length;
        module.song.restart_position = 127;
        memcpy(module.song.positions, song.positions, song.length);

        // PT: Patterns

        module.patterns = malloc(sizeof(protracker_pattern_t) * header.pattern_count);
        module.num_patterns = header.pattern_count;

        for (size_t i = 0; i < header.pattern_count; ++i)
        {
            const p61a_pattern_t* in_pattern = &(patterns[i]);
            protracker_pattern_t* out_pattern = &(module.patterns[i]);

            LOG_TRACE("Pattern #%lu:\n", i);

            for (size_t j = 0; j < PT_PATTERN_ROWS; ++j)
            {
                const p61a_pattern_row_t* in_row = &(in_pattern->rows[j]);
                protracker_pattern_row_t* out_row = &(out_pattern->rows[j]);

                LOG_TRACE("%02lu:", j);

                for (size_t k = 0; k < PT_NUM_CHANNELS; ++k)
                {
                    const p61a_channel_t* in = &(in_row->channels[k]);
                    protracker_channel_t* out = &(out_row->channels[k]);

                    to_protracker_channel(out, in);

                    char buf[32];
                    protracker_channel_to_text(out, buf, sizeof(buf));

                    LOG_TRACE(" %s", buf);
                }

                LOG_TRACE("\n");
            }
        }

        // PT: Sample Data

        const uint8_t* samples = raw + header.sample_offset;
        for (size_t i = 0; i < sample_count; ++i)
        {
            const p61a_sample_t* sample = &(sample_headers[i]);
            size_t bytes = module.sample_headers[i].length * 2;

            if (!bytes)
            {
                continue;
            }

            uint8_t* out = module.sample_data[i] = malloc(bytes);

            if (sample->length & 0x8000)
            {
                size_t source = (size_t)(0x10000 - sample->length) - 1;
                memcpy(out, module.sample_data[source], bytes);
                continue;
            }

            bool packed = packed_samples && (sample->finetone & 0x80);
            size_t stored = packed ? bytes / 2 : bytes;
            if ((size_t)(max - samples) < stored)
            {
                LOG_ERROR("Premature end of data in sample #%lu.\n", (i+1));
                valid = false;
                break;
            }

            if (packed)
            {
                uint8_t value = 0;
                for (size_t j = 0; j < stored; ++j)
                {
                    value -= (uint8_t)deltas[samples[j] >> 4];
                    out[j * 2] = value;
                    value -= (uint8_t)deltas[samples[j] & 0x0f];
                    out[j * 2 + 1] = value;
                }
            }
            else
            {
                memcpy(out, samples, bytes);

                if (delta_samples)
                {
                    for (size_t j = 1; j < bytes; ++j)
                    {
                        out[j] = out[j - 1] - out[j];
                    }
                }
            }

            samples += stored;
        }

        if (!valid)
        {
            break;
        }

        free(patterns);
        protracker_t* out = malloc(sizeof(protracker_t));
        *out = module;

        return out;
    }
    while (false);

    free(patterns);
    protracker_destroy(&module);

    return NULL;
}
