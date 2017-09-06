#include "options.h"
#include "log.h"

#include <string.h>

bool has_option(const char* options, const char* name, bool defaultValue)
{
    const char* begin = options;
    const char* end = begin + strlen(begin);
    size_t namelen = strlen(name);

    while (begin != end)
    {
        const char* curr = strstr(begin, name);
        if (!curr)
        {
            break;
        }

        if ((curr[namelen] != '\0') && (curr[namelen] != ',') && (curr[namelen] != '='))
        {
            begin = curr + 1;
            continue;
        }

        if ((begin != options) && (curr[-1] == '!'))
        {
            return false;
        }
        else
        {
            return true;
        }
    }


    return defaultValue;
}
