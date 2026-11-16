#pragma once

#include <stdbool.h>

#include "app_state.h"

void ui_config_open(void);
bool ui_config_is_open(void);
void ui_config_tick(const app_model_t *model);
