#pragma once

#include "lvgl.h"

void storage_start(void);
void storage_request(int left_band, int right_band);
void storage_next_template(void);
uint32_t storage_generation(void);
const lv_image_dsc_t *storage_left_dsc(void);
const lv_image_dsc_t *storage_right_dsc(void);
