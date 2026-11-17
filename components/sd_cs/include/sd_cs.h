#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

/* Not a wired chip-select. The SDSPI driver toggles this number; the hook
 * applies the level to CH422G EXIO4 and does not configure the pad. */
#define SD_CS_TOKEN_GPIO GPIO_NUM_6

/* level_high follows the driver's chip-select pin: true deselects the card. */
typedef void (*sd_cs_handler_t)(bool level_high);

void sd_cs_set_handler(sd_cs_handler_t handler);

esp_err_t sd_cs_gpio_set_level(gpio_num_t gpio_num, uint32_t level);
esp_err_t sd_cs_gpio_config(const gpio_config_t *config);
