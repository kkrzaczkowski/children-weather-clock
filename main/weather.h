#pragma once

#include <stdbool.h>

void weather_start(void);
void weather_request_fetch(void);
bool weather_lookup(int year, int month, int day, int hour, float *temp, float *feels, float *precip, int *prob);
