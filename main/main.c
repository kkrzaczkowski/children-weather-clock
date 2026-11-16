#include <stdio.h>

#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "board_io.h"
#include "clock_logic.h"
#include "config_store.h"
#include "log_store.h"
#include "sdcard.h"
#include "templates.h"
#include "time_sync.h"
#include "ui.h"
#include "weather.h"
#include "waveshare_rgb_lcd_port.h"
#include "wifi_mgr.h"

static const char *TAG = "children_weather_clock";

static const char *reset_text(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "włączenie zasilania";
    case ESP_RST_SW:
        return "restart programu";
    case ESP_RST_PANIC:
        return "błąd krytyczny";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_BROWNOUT:
        return "spadek napięcia";
    default:
        return "nieznany";
    }
}

void app_main(void)
{
    log_init();
    app_state_init();
    sdcard_init();
    config_store_init();
    const char *logic_error = clock_logic_self_test();
    if (logic_error != NULL) {
        log_event(LOG_ERROR, "Test logiki nie przeszedł: %s", logic_error);
    }
    log_event(LOG_INFO, "Uruchomienie: %s", reset_text(esp_reset_reason()));

    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(tear_mode, rotation, &panel_handle, &touch_handle));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());
    board_io_init();

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 32 * 1024;
    adapter_config.stack_in_psram = true;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel_handle, NULL, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, rotation);
    display_config.profile.use_psram = true;
    lv_display_t *display = esp_lv_adapter_register_display(&display_config);
    if (display == NULL) {
        log_event(LOG_ERROR, "Ekran nie został zarejestrowany");
        return;
    }
    if (touch_handle != NULL) {
        esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch_handle);
        lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
        if (touch == NULL) {
            log_event(LOG_WARN, "Dotyk nie został zarejestrowany");
        }
    }
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_init();
        esp_lv_adapter_unlock();
    }
    /* Wi-Fi claims its DMA buffers before the SD bus starts, so a missing card cannot starve the radio. */
    wifi_start();
    storage_start();
    time_sync_start();
    weather_start();
    ESP_LOGI(TAG, "Zegar pogodowy uruchomiony");
}
