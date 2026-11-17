#include "sd_cs.h"

static sd_cs_handler_t s_handler;

void sd_cs_set_handler(sd_cs_handler_t handler)
{
    s_handler = handler;
}

esp_err_t sd_cs_gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    if (gpio_num == SD_CS_TOKEN_GPIO) {
        if (s_handler != NULL) {
            s_handler(level != 0);
        }
        return ESP_OK;
    }
    return gpio_set_level(gpio_num, level);
}

esp_err_t sd_cs_gpio_config(const gpio_config_t *config)
{
    if (config == NULL) {
        return gpio_config(config);
    }
    const uint64_t token = 1ULL << SD_CS_TOKEN_GPIO;
    if ((config->pin_bit_mask & token) == 0) {
        return gpio_config(config);
    }
    gpio_config_t rest = *config;
    rest.pin_bit_mask &= ~token;
    if (rest.pin_bit_mask == 0) {
        return ESP_OK;
    }
    return gpio_config(&rest);
}
