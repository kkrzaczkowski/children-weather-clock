#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
} log_level_t;

void log_init(void);
void log_bind_storage(bool enabled);
void log_event(log_level_t level, const char *fmt, ...);
bool log_safe_name(const char *name);
int log_list(char names[][32], int capacity);
