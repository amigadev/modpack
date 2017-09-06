#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static int log_level = LOG_INFO;

void set_log_level(int new_level)
{
    log_level = new_level;
}

void log_msg(int level, const char* format, ...)
{
    if (level > log_level)
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
