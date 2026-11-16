#pragma once

#include <stddef.h>
#include <stdint.h>

#include "clock_logic.h"

bool image_decode_png(const uint8_t *data, size_t length, uint16_t *dest);
void image_fill_fallback(uint16_t *dest, int band);
