#pragma once

#include <stdbool.h>

typedef enum {
    SD_OK = 0,
    SD_MISSING,
    SD_FULL,
} sd_status_t;

void sdcard_init(void);
sd_status_t sdcard_mount(void);
const char *sdcard_last_error(void);
void sdcard_lock(void);
void sdcard_unlock(void);
bool sdcard_mounted(void);
