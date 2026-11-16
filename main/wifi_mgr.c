#include "wifi_mgr.h"

#include <string.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#include "app_state.h"
#include "http_panel.h"
#include "log_store.h"
#include "secrets.h"
#include "time_sync.h"
#include "weather.h"

static const char *TAG = "wifi";
static esp_timer_handle_t s_reconnect;

static void reconnect_cb(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static const char *reason_text(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "Nie można połączyć z zapisaną siecią";
    case WIFI_REASON_NO_AP_FOUND:
        return "Nie znaleziono sieci Wi-Fi";
    default:
        return "Brak połączenia z siecią";
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        const char *text = reason_text(event->reason);
        app_state_lock();
        app_model_t *model = app_state_edit();
        model->wifi_up = false;
        model->ip[0] = '\0';
        snprintf(model->wifi_error, sizeof(model->wifi_error), "%s", text);
        app_state_bump();
        app_state_unlock();
        time_sync_wifi(false);
        log_event(LOG_WARN, "Wi-Fi: %s", text);
        if (s_reconnect != NULL) {
            esp_timer_stop(s_reconnect);
            esp_timer_start_once(s_reconnect, 2000000);
        }
        return;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        app_state_lock();
        app_model_t *model = app_state_edit();
        model->wifi_up = true;
        snprintf(model->ip, sizeof(model->ip), "%s", ip);
        model->wifi_error[0] = '\0';
        app_state_bump();
        app_state_unlock();
        time_sync_wifi(true);
        weather_request_fetch();
        http_panel_start();
        log_event(LOG_INFO, "Wi-Fi: połączono, adres %s", ip);
    }
}

static void remember_failure(const char *text)
{
    app_state_lock();
    app_model_t *model = app_state_edit();
    model->wifi_up = false;
    model->ip[0] = '\0';
    snprintf(model->wifi_error, sizeof(model->wifi_error), "%s", text);
    app_state_bump();
    app_state_unlock();
    log_event(LOG_ERROR, "Wi-Fi: %s", text);
}

static bool wifi_step(esp_err_t err, const char *what, bool started)
{
    if (err == ESP_OK) {
        return true;
    }
    ESP_LOGE(TAG, "%s: %s", what, esp_err_to_name(err));
    if (started) {
        esp_err_t stop = esp_wifi_stop();
        if (stop != ESP_OK && stop != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_LOGW(TAG, "wifi stop: %s", esp_err_to_name(stop));
        }
        esp_wifi_deinit();
    }
    remember_failure("Wi-Fi nie działa");
    return false;
}

void wifi_start(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif: %s", esp_err_to_name(err));
        remember_failure("Wi-Fi nie działa");
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        remember_failure("Wi-Fi nie działa");
        return;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    /* A weather clock does not need the default high-throughput buffers. */
    init.static_rx_buf_num = 6;
    if (init.dynamic_rx_buf_num > 16) {
        init.dynamic_rx_buf_num = 16;
    }
    if (init.dynamic_tx_buf_num > 16) {
        init.dynamic_tx_buf_num = 16;
    }
    ESP_LOGI(TAG, "Wolna pamięć wewnętrzna: %u B, największy blok DMA: %u B",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    if (!wifi_step(esp_wifi_init(&init), "esp_wifi_init", false)) {
        return;
    }
    if (!wifi_step(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL), "wifi handler", true)) {
        return;
    }
    if (!wifi_step(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, NULL), "ip handler", true)) {
        return;
    }
    const esp_timer_create_args_t timer_args = {
        .callback = reconnect_cb,
        .name = "wifi_retry",
    };
    if (!wifi_step(esp_timer_create(&timer_args, &s_reconnect), "wifi timer", true)) {
        return;
    }

    wifi_config_t wifi = {0};
    snprintf((char *)wifi.sta.ssid, sizeof(wifi.sta.ssid), "%s", APP_WIFI_SSID);
    snprintf((char *)wifi.sta.password, sizeof(wifi.sta.password), "%s", APP_WIFI_PASSWORD);
    wifi.sta.threshold.authmode = APP_WIFI_PASSWORD[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    if (!wifi_step(esp_wifi_set_mode(WIFI_MODE_STA), "wifi mode", true)) {
        return;
    }
    if (!wifi_step(esp_wifi_set_config(WIFI_IF_STA, &wifi), "wifi config", true)) {
        return;
    }
    if (!wifi_step(esp_wifi_start(), "wifi start", true)) {
        return;
    }
    ESP_LOGI(TAG, "Łączenie z siecią %s", APP_WIFI_SSID);
    log_event(LOG_INFO, "Wi-Fi: łączenie z %s", APP_WIFI_SSID);
}
