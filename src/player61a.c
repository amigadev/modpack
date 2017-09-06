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

static void build_samples(player61a_t* output, const protracker_t* module)
{
    for (size_t i = 0; i < PT_NUM_SAMPLES; ++i)
    {
        uint8_t sample = (uint8_t)i+1;
        const protracker_sample_t* input = &(module->sample_headers[i]);

    }
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

bool player61a_convert(buffer_t* buffer, const protracker_t* module, const char* options)
{
    log_msg(LOG_INFO, "Converting to The Player 6.1A...\n");

    player61a_t output = { 0 };

    build_samples(&output, module);

    if (has_option(options, "sign", false))
    {
        log_msg(LOG_TRACE, " - Adding signature.\n");
        buffer_add(buffer, signature, strlen(signature));
    }


    return false;
}
