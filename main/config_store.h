#pragma once

#include <stdbool.h>

#include "clock_logic.h"

void config_store_init(void);
bool config_store_save(const clock_config_t *cfg);
