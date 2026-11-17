#include "board_io.h"

#include "driver/i2c.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_cs.h"
#include "waveshare_rgb_lcd_port.h"

static const char *TAG = "board_io";
static uint8_t s_shadow = 0x1E;
static bool s_ready = false;
static SemaphoreHandle_t s_lock;
/* The storage task stack is in PSRAM. The I2C command must stay in internal RAM. */
static DRAM_ATTR uint8_t s_i2c_cmd[I2C_LINK_RECOMMENDED_SIZE(1)];
static DRAM_ATTR uint8_t s_i2c_byte;

static void tf_cs_from_driver(bool level_high)
{
    /* Driver level 0 selects the card. EXIO4 is active low. */
    if (!board_io_sd_select(!level_high)) {
        ESP_LOGW(TAG, "TF chip select failed");
    }
}

static void ch422g_write(uint8_t value)
{
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &value, 1, pdMS_TO_TICKS(100));
}

void board_io_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    uint8_t mode = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &mode, 1, pdMS_TO_TICKS(100));
    s_shadow = 0x1E;
    ch422g_write(s_shadow);
    s_ready = true;
    sd_cs_set_handler(tf_cs_from_driver);
}

bool board_io_sd_select(bool selected)
{
    if (!s_ready) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (selected) {
        s_shadow &= (uint8_t)~(1u << 4);
    } else {
        s_shadow |= (uint8_t)(1u << 4);
    }
    s_i2c_byte = s_shadow;
    i2c_cmd_handle_t handle = i2c_cmd_link_create_static(s_i2c_cmd, sizeof(s_i2c_cmd));
    esp_err_t err = ESP_ERR_NO_MEM;
    if (handle != NULL) {
        err = i2c_master_start(handle);
        if (err == ESP_OK) {
            err = i2c_master_write_byte(handle, (0x38 << 1) | I2C_MASTER_WRITE, true);
        }
        if (err == ESP_OK) {
            err = i2c_master_write(handle, &s_i2c_byte, 1, true);
        }
        if (err == ESP_OK) {
            err = i2c_master_stop(handle);
        }
        if (err == ESP_OK) {
            err = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(100));
        }
        i2c_cmd_link_delete_static(handle);
    }
    xSemaphoreGive(s_lock);
    return err == ESP_OK;
}
