#include "board_io.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "waveshare_rgb_lcd_port.h"

static uint8_t s_shadow = 0x1E;
static bool s_ready = false;
static SemaphoreHandle_t s_lock;

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
}

void board_io_mirror_cs(uint32_t level)
{
    if (!s_ready) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (level) {
        s_shadow |= (uint8_t)(1u << 4);
    } else {
        s_shadow &= (uint8_t)~(1u << 4);
    }
    uint8_t value = s_shadow;
    xSemaphoreGive(s_lock);
    ch422g_write(value);
}

esp_err_t __real_gpio_set_level(gpio_num_t gpio_num, uint32_t level);

esp_err_t __wrap_gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    if (gpio_num == GPIO_NUM_6) {
        board_io_mirror_cs(level);
    }
    return __real_gpio_set_level(gpio_num, level);
}
