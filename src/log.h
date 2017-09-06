#pragma once

#define LOG_NONE (-3)
#define LOG_ERROR (-2)
#define LOG_WARN (-1)
#define LOG_INFO (0)
#define LOG_DEBUG (1)
#define LOG_TRACE (2)

void set_log_level(int level);
void log_msg(int level, const char* format, ...);
