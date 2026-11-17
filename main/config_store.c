#include "config_store.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "log_store.h"

static const char *TAG = "config";

typedef struct {
    clock_config_t cfg;
    bool ok;
    SemaphoreHandle_t done;
} save_request_t;

static QueueHandle_t s_save_queue;

static void remember(const clock_config_t *cfg, const char *error)
{
    app_state_lock();
    app_model_t *model = app_state_edit();
    model->cfg = *cfg;
    snprintf(model->config_error, sizeof(model->config_error), "%s", error != NULL ? error : "");
    app_state_bump();
    app_state_unlock();
}

static bool write_config(const clock_config_t *cfg)
{
    const char *message = NULL;
    clock_config_t clean = *cfg;
    clean.schema_version = CLOCK_SCHEMA_VERSION;
    if (!clock_config_validate(&clean, &message)) {
        log_event(LOG_WARN, "Konfiguracja odrzucona: %s", message != NULL ? message : "błąd");
        return false;
    }

    uint8_t raw[128];
    size_t len = clock_config_encode(&clean, raw, sizeof(raw));
    if (len == 0) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("clock", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        log_event(LOG_ERROR, "Konfiguracja: zapis niemożliwy");
        return false;
    }
    err = nvs_set_blob(handle, "cfg", raw, len);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        log_event(LOG_ERROR, "Konfiguracja: commit nieudany");
        return false;
    }

    remember(&clean, "");
    log_event(LOG_INFO, "Konfiguracja zapisana, interwał %u min, prognoza %u:00, szablon %s",
              clean.refresh_minutes, clean.forecast_hour,
              clean.template_name[0] != '\0' ? clean.template_name : "(awaryjny)");
    return true;
}

static void nvs_save_task(void *arg)
{
    (void)arg;
    for (;;) {
        save_request_t *req = NULL;
        if (xQueueReceive(s_save_queue, &req, portMAX_DELAY) != pdTRUE || req == NULL) {
            continue;
        }
        req->ok = write_config(&req->cfg);
        xSemaphoreGive(req->done);
    }
}

static void start_save_task(void)
{
    if (s_save_queue != NULL) {
        return;
    }
    s_save_queue = xQueueCreate(1, sizeof(save_request_t *));
    if (s_save_queue == NULL) {
        ESP_LOGE(TAG, "NVS: brak kolejki zapisu");
        return;
    }
    if (xTaskCreate(nvs_save_task, "nvs_save", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "NVS: brak zadania zapisu");
        vQueueDelete(s_save_queue);
        s_save_queue = NULL;
    }
}

void config_store_init(void)
{
    start_save_task();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init required erase");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        clock_config_t cfg;
        clock_config_defaults(&cfg);
        remember(&cfg, "Uszkodzona konfiguracja. Użyto wartości domyślnych.");
        log_event(LOG_ERROR, "Konfiguracja: nie można otworzyć NVS");
        return;
    }

    nvs_handle_t handle;
    err = nvs_open("clock", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        clock_config_t cfg;
        clock_config_defaults(&cfg);
        remember(&cfg, "");
        log_event(LOG_INFO, "Konfiguracja: wartości domyślne");
        return;
    }

    uint8_t raw[128];
    size_t len = sizeof(raw);
    err = nvs_get_blob(handle, "cfg", raw, &len);
    nvs_close(handle);
    clock_config_t cfg;
    bool migrated = false;
    if (err != ESP_OK || !clock_config_decode(raw, len, &cfg, &migrated)) {
        clock_config_defaults(&cfg);
        remember(&cfg, "Uszkodzona konfiguracja. Użyto wartości domyślnych.");
        log_event(LOG_ERROR, "Konfiguracja: uszkodzony zapis, użyto wartości domyślnych");
        return;
    }

    bool repaired = clock_config_repair(&cfg);
    remember(&cfg, repaired || migrated ? "Przywrócono część ustawień do wartości domyślnych." : "");
    if (repaired || migrated) {
        write_config(&cfg);
        log_event(LOG_WARN, "Konfiguracja: migracja albo poprawa pól spoza zakresu");
    } else {
        log_event(LOG_INFO, "Konfiguracja: odczytana");
    }
}

bool config_store_save(const clock_config_t *cfg)
{
    if (cfg == NULL || s_save_queue == NULL) {
        log_event(LOG_ERROR, "Konfiguracja: zapis niemożliwy");
        return false;
    }
    save_request_t req = {
        .cfg = *cfg,
        .ok = false,
        .done = xSemaphoreCreateBinary(),
    };
    if (req.done == NULL) {
        log_event(LOG_ERROR, "Konfiguracja: zapis niemożliwy");
        return false;
    }
    save_request_t *slot = &req;
    xQueueSend(s_save_queue, &slot, portMAX_DELAY);
    xSemaphoreTake(req.done, portMAX_DELAY);
    bool ok = req.ok;
    vSemaphoreDelete(req.done);
    return ok;
}
