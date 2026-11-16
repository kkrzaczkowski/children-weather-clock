#include "time_sync.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_state.h"
#include "clock_logic.h"
#include "log_store.h"
#include "weather.h"

static const char *TAG = "time";
static volatile bool s_synced;
static volatile bool s_wifi;

bool time_is_synced(void)
{
    return s_synced;
}

void time_sync_wifi(bool up)
{
    s_wifi = up;
}

static bool sync_once(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP init %s", esp_err_to_name(err));
        return false;
    }
    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(20000));
    esp_netif_sntp_deinit();
    if (err != ESP_OK) {
        log_event(LOG_WARN, "NTP: brak odpowiedzi");
        return false;
    }
    time_t now = time(NULL);
    if (now < 1700000000) {
        log_event(LOG_WARN, "NTP: czas nieprawdopodobny");
        return false;
    }
    s_synced = true;
    app_state_lock();
    app_model_t *model = app_state_edit();
    model->time_synced = true;
    model->time_error[0] = '\0';
    app_state_bump();
    app_state_unlock();
    log_event(LOG_INFO, "NTP: czas zsynchronizowany");
    weather_request_fetch();
    return true;
}

static void time_task(void *arg)
{
    (void)arg;
    setenv("TZ", CLOCK_TZ_POSIX, 1);
    tzset();
    time_t last_ok = 0;
    bool wifi_was_down = true;

    while (true) {
        if (!s_wifi) {
            wifi_was_down = true;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        time_t now = time(NULL);
        bool due = !s_synced || wifi_was_down || (last_ok != 0 && (now - last_ok) >= CLOCK_NTP_PERIOD_S);
        wifi_was_down = false;
        if (due) {
            if (sync_once()) {
                last_ok = time(NULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void time_sync_start(void)
{
    xTaskCreate(time_task, "time", 6144, NULL, 4, NULL);
}
