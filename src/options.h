#pragma once

#include <stdbool.h>
#include <stddef.h>

bool has_option(const char* options, const char* name, bool defaultValue);
bool get_option_range(const char* options, const char* name, bool* range, size_t count);
