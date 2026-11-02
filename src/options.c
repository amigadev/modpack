#include "options.h"
#include "log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* find_option(const char* options, const char* name)
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

        return curr;
    }

    return NULL;
}

bool has_option(const char* options, const char* name, bool defaultValue)
{
    const char* curr = find_option(options, name);
    if (!curr)
    {
        return defaultValue;
    }

    return !((curr != options) && (curr[-1] == '-'));
}

bool get_option_range(const char* options, const char* name, bool* range, size_t count)
{
    memset(range, 0, count * sizeof(bool));

    const char* curr = find_option(options, name);
    if (!curr || ((curr != options) && (curr[-1] == '-')))
    {
        return true;
    }

    curr += strlen(name);
    if (*curr != '=')
    {
        memset(range, 1, count * sizeof(bool));
        return true;
    }

    ++curr;
    bool brackets = *curr == '[';
    if (brackets)
    {
        ++curr;
    }

    while (true)
    {
        char* next;
        unsigned long first = strtoul(curr, &next, 10);
        unsigned long last = first;
        if (next == curr)
        {
            break;
        }
        curr = next;

        if (*curr == '-')
        {
            ++curr;
            last = strtoul(curr, &next, 10);
            if (next == curr)
            {
                break;
            }
            curr = next;
        }

        if (!first || first > last || last > count)
        {
            break;
        }

        for (unsigned long i = first; i <= last; ++i)
        {
            range[i-1] = true;
        }

        if (*curr == ':')
        {
            ++curr;
            continue;
        }

        if (brackets && (*curr++ != ']'))
        {
            break;
        }

        if ((*curr == '\0') || (*curr == ','))
        {
            return true;
        }

        break;
    }

    LOG_ERROR("Invalid range for option '%s'.\n", name);
    return false;
}
