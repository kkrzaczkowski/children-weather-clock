#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLOCK_SCHEMA_VERSION 1
#define CLOCK_TEMPLATE_NAME_LEN 48
#define CLOCK_TEMPLATE_MAX 16

#define CLOCK_LATITUDE_STR "50.09"
#define CLOCK_LONGITUDE_STR "18.22"
#define CLOCK_LOCATION_NAME "Racibórz"
#define CLOCK_TIMEZONE_NAME "Europe/Warsaw"
#define CLOCK_TZ_POSIX "CET-1CEST,M3.5.0,M10.5.0/3"

#define CLOCK_REFRESH_DEFAULT 120
#define CLOCK_FORECAST_HOUR_MIN 8
#define CLOCK_FORECAST_HOUR_MAX 20
#define CLOCK_FORECAST_HOUR_DEFAULT 15
#define CLOCK_BRIGHTNESS_MIN 10
#define CLOCK_BRIGHTNESS_MAX 100
#define CLOCK_BRIGHTNESS_STEP 10
#define CLOCK_BRIGHTNESS_DAY_DEFAULT 100
#define CLOCK_BRIGHTNESS_NIGHT_DEFAULT 30
#define CLOCK_DIM_START_DEFAULT (20 * 60)
#define CLOCK_DIM_END_DEFAULT (6 * 60)

#define CLOCK_IMAGE_W 266
#define CLOCK_IMAGE_H 480
#define CLOCK_IMAGE_MAX_SIDE 960
#define CLOCK_IMAGE_MAX_BYTES (400 * 1024)

#define CLOCK_GESTURE_PX 120
#define CLOCK_GESTURE_MS 1500
#define CLOCK_CONFIG_IDLE_MS (120 * 1000)
#define CLOCK_NTP_PERIOD_S (24 * 60 * 60)
#define CLOCK_RETRY_COUNT 3

typedef struct {
    uint16_t schema_version;
    uint16_t refresh_minutes;
    uint16_t dim_start_min;
    uint16_t dim_end_min;
    uint8_t brightness_day;
    uint8_t brightness_night;
    uint8_t forecast_hour;
    uint8_t font_clock;
    uint8_t font_temp;
    uint8_t font_label;
    char template_name[CLOCK_TEMPLATE_NAME_LEN];
} clock_config_t;

typedef enum {
    FORECAST_TODAY = 0,
    FORECAST_TOMORROW = 1,
} forecast_kind_t;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    forecast_kind_t kind;
} forecast_slot_t;

typedef enum {
    WMO_SUN = 0,
    WMO_PARTLY,
    WMO_CLOUD,
    WMO_FOG,
    WMO_RAIN,
    WMO_FREEZE,
    WMO_SNOW,
    WMO_STORM,
    WMO_UNKNOWN,
} wmo_icon_t;

void clock_config_defaults(clock_config_t *cfg);
bool clock_refresh_allowed(int minutes);
bool clock_brightness_allowed(int percent);
bool clock_template_name_ok(const char *name);
int clock_name_cmp(const char *a, const char *b);

/* Returns false and sets a short Polish message when the draft cannot be saved. */
bool clock_config_validate(const clock_config_t *cfg, const char **message);

/* Replaces out-of-range fields with defaults. Returns true when anything changed. */
bool clock_config_repair(clock_config_t *cfg);

/* Blob is little-endian and includes a CRC. migrated is set when an older schema was read. */
bool clock_config_decode(const uint8_t *raw, size_t len, clock_config_t *out, bool *migrated);
size_t clock_config_encode(const clock_config_t *cfg, uint8_t *out, size_t cap);

int clock_round_half_away(float value);
int clock_temp_band(int rounded);
int clock_temp_color(int actual_rounded, int feels_rounded);

void forecast_select(int year, int month, int day, int hour, int minute, int forecast_hour, forecast_slot_t *out);
bool brightness_is_dim(int minute_of_day, int start_min, int end_min);
wmo_icon_t wmo_icon(int code);

uint32_t clock_crc32(const void *data, size_t len);

/* NULL when every check passed, otherwise a short reason. */
const char *clock_logic_self_test(void);
