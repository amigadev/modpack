#include "converter.h"
#include "player61a.h"

int convert(buffer_t* output, const protracker_t* module, int format)
{
    switch (format)
    {
        case CONVERT_FORMAT_PROTRACKER:
        {
            fprintf(stderr, "Converting to ProTracker...\n");
            return protracker_convert(output, module);
        }
        break;

        case CONVERT_FORMAT_PLAYER61A:
        {
            fprintf(stderr, "Converting to The Player 6.1A...\n");
            return player61a_convert(output, module);
        }
        break;

        default:
        {
            fprintf(stderr, "Unsupported format: %d\n", format);
            return -1;
        }
    }
}

