#pragma once

#include "buffer.h"
#include "protracker.h"

#define CONVERT_FORMAT_PROTRACKER   (0)
#define CONVERT_FORMAT_PLAYER61A    (1)

int convert(buffer_t* output, const protracker_t* module, int format);
