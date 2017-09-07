#pragma once

#define LOG_LEVEL_NONE (-3)
#define LOG_LEVEL_ERROR (-2)
#define LOG_LEVEL_WARN (-1)
#define LOG_LEVEL_INFO (0)
#define LOG_LEVEL_DEBUG (1)
#define LOG_LEVEL_TRACE (2)

void set_log_level(int level);
void log_msg(int level, const char* format, ...);

#define LOG_ERROR(...) log_msg(LOG_LEVEL_ERROR, "ERROR: " __VA_ARGS__)
#define LOG_WARN(...) log_msg(LOG_LEVEL_WARN, "WARN: " __VA_ARGS__)
#define LOG_INFO(...) log_msg(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_msg(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_TRACE(...) log_msg(LOG_LEVEL_TRACE, __VA_ARGS__)
