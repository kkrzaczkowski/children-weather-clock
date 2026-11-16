#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "clock_logic.h"

typedef struct {
    clock_config_t cfg;
    char config_error[120];
    bool wifi_up;
    char ip[16];
    char wifi_error[96];
    bool time_synced;
    char time_error[96];
    bool weather_ok;
    time_t weather_at;
    float cur_temp;
    float cur_feels;
    float cur_precip;
    int cur_code;
    char weather_error[120];
    bool sd_ok;
    bool sd_full;
    bool logs_on;
    char sd_error[96];
    char template_note[96];
    char templates[CLOCK_TEMPLATE_MAX][CLOCK_TEMPLATE_NAME_LEN];
    int template_count;
    uint32_t epoch;
} app_model_t;

void app_state_init(void);
void app_state_lock(void);
void app_state_unlock(void);
void app_state_copy(app_model_t *out);
app_model_t *app_state_edit(void);
void app_state_bump(void);
