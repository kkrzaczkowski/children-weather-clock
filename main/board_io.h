#pragma once

#include <stdbool.h>
#include <stdint.h>

void board_io_init(void);
/* Active-low TF chip select on CH422G EXIO4. Called for each SDSPI transfer. */
bool board_io_sd_select(bool selected);
