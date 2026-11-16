#pragma once

#include <stdbool.h>

void time_sync_start(void);
bool time_is_synced(void);
void time_sync_wifi(bool up);
