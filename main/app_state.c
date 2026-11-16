#include "app_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static app_model_t s_model;
static SemaphoreHandle_t s_lock;

void app_state_init(void)
{
    memset(&s_model, 0, sizeof(s_model));
    clock_config_defaults(&s_model.cfg);
    s_model.cur_code = -1;
    s_lock = xSemaphoreCreateMutex();
}

void app_state_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_lock);
}

void app_state_copy(app_model_t *out)
{
    app_state_lock();
    *out = s_model;
    app_state_unlock();
}

app_model_t *app_state_edit(void)
{
    return &s_model;
}

void app_state_bump(void)
{
    s_model.epoch++;
}
