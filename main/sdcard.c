#include "sdcard.h"

#include "board_io.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_cs.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sdcard";
static SemaphoreHandle_t s_lock;
static sdmmc_card_t *s_card;
static bool s_mounted;
static const char *s_error = "";

const char *sdcard_last_error(void)
{
    return s_error;
}

void sdcard_init(void)
{
    s_lock = xSemaphoreCreateMutex();
}

void sdcard_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

void sdcard_unlock(void)
{
    xSemaphoreGive(s_lock);
}

bool sdcard_mounted(void)
{
    return s_mounted;
}

sd_status_t sdcard_mount(void)
{
    /* EXIO4 stays high until the driver selects the card. board_io_init left bit 4 set. */
    spi_bus_config_t bus = {
        .mosi_io_num = GPIO_NUM_11,
        .miso_io_num = GPIO_NUM_13,
        .sclk_io_num = GPIO_NUM_12,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    bool own_bus = err == ESP_OK;
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI init failed: %s", esp_err_to_name(err));
        board_io_sd_select(false);
        s_error = "Karta pamięci nie działa";
        return SD_MISSING;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000;
    /* This card rejects CMD59 while it is still idle. Leave data CRC off and continue. */
    host.flags |= SDMMC_HOST_FLAG_SPI_IGNORE_DATA_CRC;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = SPI2_HOST;
    /* Token only. sd_cs applies these edges to EXIO4 and does not drive GPIO6. */
    slot.gpio_cs = SD_CS_TOKEN_GPIO;

    esp_vfs_fat_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TF mount failed: %s", esp_err_to_name(err));
        if (own_bus) {
            spi_bus_free(SPI2_HOST);
        }
        board_io_sd_select(false);
        s_error = (err == ESP_ERR_TIMEOUT || err == ESP_ERR_NOT_FOUND) ? "Brak karty pamięci" : "Karta pamięci nie działa";
        return SD_MISSING;
    }

    uint64_t total = 0;
    uint64_t free_bytes = 0;
    if (esp_vfs_fat_info("/sdcard", &total, &free_bytes) == ESP_OK && free_bytes < 64 * 1024) {
        ESP_LOGW(TAG, "TF card full");
        s_mounted = true;
        s_error = "Karta pamięci jest pełna";
        return SD_FULL;
    }

    s_error = "";
    s_mounted = true;
    ESP_LOGI(TAG, "TF card mounted");
    return SD_OK;
}
