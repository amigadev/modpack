#pragma once

#define LOG_LEVEL_NONE (-3)
#define LOG_LEVEL_ERROR (-2)
#define LOG_LEVEL_WARN (-1)
#define LOG_LEVEL_INFO (0)
#define LOG_LEVEL_DEBUG (1)
#define LOG_LEVEL_TRACE (2)

void set_log_level(int level);
void log_msg(int level, const char* format, ...);

#define LOG_ERROR(instance, ...) log_msg(instance, LOG_LEVEL_ERROR, "ERROR: " __VA_ARGS__)
#define LOG_WARN(instance, ...) log_msg(instance, LOG_LEVEL_WARN, "WARN: " __VA_ARGS__)
#define LOG_INFO(instance, ...) log_msg(instance, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(instance, ...) log_msg(instance, LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_TRACE(instance, ...) log_msg(instance, LOG_LEVEL_TRACE, __VA_ARGS__)
