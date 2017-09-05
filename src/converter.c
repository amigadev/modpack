#include "converter.h"
#include "player61a.h"
#include "log.h"

int convert(buffer_t* output, const protracker_t* module, int format)
{
    switch (format)
    {
        case CONVERT_FORMAT_PROTRACKER:
        {
            log_msg(LOG_INFO, "Converting to ProTracker...\n");
            return protracker_convert(output, module);
        }
        break;

        case CONVERT_FORMAT_PLAYER61A:
        {
            log_msg(LOG_INFO, "Converting to The Player 6.1A...\n");
            return player61a_convert(output, module);
        }
        break;

        default:
        {
            log_msg(LOG_INFO, "Unsupported format: %d\n", format);
            return -1;
        }
    }
}

