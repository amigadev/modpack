#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

void debug(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
