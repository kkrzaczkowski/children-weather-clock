#include "clock_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* version, refresh, dim start, dim end, six bytes, name, crc */
#define CLOCK_BLOB_V1_BODY (2 + 2 + 2 + 2 + 6 + CLOCK_TEMPLATE_NAME_LEN)
#define CLOCK_BLOB_V1_SIZE (CLOCK_BLOB_V1_BODY + 4)

uint32_t clock_crc32(const void *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = data;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void clock_config_defaults(clock_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->schema_version = CLOCK_SCHEMA_VERSION;
    cfg->refresh_minutes = CLOCK_REFRESH_DEFAULT;
    cfg->dim_start_min = CLOCK_DIM_START_DEFAULT;
    cfg->dim_end_min = CLOCK_DIM_END_DEFAULT;
    cfg->brightness_day = CLOCK_BRIGHTNESS_DAY_DEFAULT;
    cfg->brightness_night = CLOCK_BRIGHTNESS_NIGHT_DEFAULT;
    cfg->forecast_hour = CLOCK_FORECAST_HOUR_DEFAULT;
    cfg->font_clock = 1;
    cfg->font_temp = 1;
    cfg->font_label = 1;
}

bool clock_refresh_allowed(int minutes)
{
    return minutes == 30 || minutes == 60 || minutes == 120 || minutes == 180 || minutes == 360;
}

bool clock_brightness_allowed(int percent)
{
    return percent >= CLOCK_BRIGHTNESS_MIN && percent <= CLOCK_BRIGHTNESS_MAX &&
           (percent % CLOCK_BRIGHTNESS_STEP) == 0;
}

int clock_name_cmp(const char *a, const char *b)
{
    if (a == NULL) {
        a = "";
    }
    if (b == NULL) {
        b = "";
    }
    while (*a != '\0' && *b != '\0') {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca + 32);
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb + 32);
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

bool clock_template_name_ok(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return true;
    }
    size_t n = strlen(name);
    if (n >= CLOCK_TEMPLATE_NAME_LEN) {
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            return false;
        }
    }
    return true;
}

bool clock_config_validate(const clock_config_t *cfg, const char **message)
{
    if (message != NULL) {
        *message = NULL;
    }
    if (cfg == NULL) {
        if (message != NULL) {
            *message = "Brak konfiguracji";
        }
        return false;
    }
    if (!clock_refresh_allowed(cfg->refresh_minutes)) {
        if (message != NULL) {
            *message = "Wybierz interwał z listy";
        }
        return false;
    }
    if (!clock_brightness_allowed(cfg->brightness_day) || !clock_brightness_allowed(cfg->brightness_night)) {
        if (message != NULL) {
            *message = "Jasność od 10% do 100% co 10%";
        }
        return false;
    }
    if (cfg->dim_start_min > 1439 || cfg->dim_end_min > 1439) {
        if (message != NULL) {
            *message = "Godzina poza zakresem";
        }
        return false;
    }
    if (cfg->dim_start_min == cfg->dim_end_min) {
        if (message != NULL) {
            *message = "Początek i koniec muszą być różne";
        }
        return false;
    }
    if (cfg->forecast_hour < CLOCK_FORECAST_HOUR_MIN || cfg->forecast_hour > CLOCK_FORECAST_HOUR_MAX) {
        if (message != NULL) {
            *message = "Godzina prognozy od 8 do 20";
        }
        return false;
    }
    if (cfg->font_clock > 2 || cfg->font_temp > 2 || cfg->font_label > 2) {
        if (message != NULL) {
            *message = "Wybierz rozmiar czcionki";
        }
        return false;
    }
    if (!clock_template_name_ok(cfg->template_name)) {
        if (message != NULL) {
            *message = "Niedozwolona nazwa szablonu";
        }
        return false;
    }
    return true;
}

bool clock_config_repair(clock_config_t *cfg)
{
    clock_config_t defaults;
    clock_config_defaults(&defaults);
    bool changed = false;

    if (!clock_refresh_allowed(cfg->refresh_minutes)) {
        cfg->refresh_minutes = defaults.refresh_minutes;
        changed = true;
    }
    if (!clock_brightness_allowed(cfg->brightness_day)) {
        cfg->brightness_day = defaults.brightness_day;
        changed = true;
    }
    if (!clock_brightness_allowed(cfg->brightness_night)) {
        cfg->brightness_night = defaults.brightness_night;
        changed = true;
    }
    if (cfg->dim_start_min > 1439 || cfg->dim_end_min > 1439 || cfg->dim_start_min == cfg->dim_end_min) {
        cfg->dim_start_min = defaults.dim_start_min;
        cfg->dim_end_min = defaults.dim_end_min;
        changed = true;
    }
    if (cfg->forecast_hour < CLOCK_FORECAST_HOUR_MIN || cfg->forecast_hour > CLOCK_FORECAST_HOUR_MAX) {
        cfg->forecast_hour = defaults.forecast_hour;
        changed = true;
    }
    if (cfg->font_clock > 2) {
        cfg->font_clock = defaults.font_clock;
        changed = true;
    }
    if (cfg->font_temp > 2) {
        cfg->font_temp = defaults.font_temp;
        changed = true;
    }
    if (cfg->font_label > 2) {
        cfg->font_label = defaults.font_label;
        changed = true;
    }
    if (!clock_template_name_ok(cfg->template_name)) {
        cfg->template_name[0] = '\0';
        changed = true;
    }
    cfg->schema_version = CLOCK_SCHEMA_VERSION;
    return changed;
}

static void decode_v1_body(const uint8_t *raw, clock_config_t *out)
{
    clock_config_defaults(out);
    out->schema_version = rd_u16(raw);
    out->refresh_minutes = rd_u16(raw + 2);
    out->dim_start_min = rd_u16(raw + 4);
    out->dim_end_min = rd_u16(raw + 6);
    out->brightness_day = raw[8];
    out->brightness_night = raw[9];
    out->forecast_hour = raw[10];
    out->font_clock = raw[11];
    out->font_temp = raw[12];
    out->font_label = raw[13];
    memcpy(out->template_name, raw + 14, CLOCK_TEMPLATE_NAME_LEN);
    out->template_name[CLOCK_TEMPLATE_NAME_LEN - 1] = '\0';
}

bool clock_config_decode(const uint8_t *raw, size_t len, clock_config_t *out, bool *migrated)
{
    if (migrated != NULL) {
        *migrated = false;
    }
    if (raw == NULL || out == NULL || len < 4) {
        return false;
    }

    uint16_t version = rd_u16(raw);
    if (version == CLOCK_SCHEMA_VERSION) {
        if (len != CLOCK_BLOB_V1_SIZE) {
            return false;
        }
        uint32_t expect = clock_crc32(raw, CLOCK_BLOB_V1_BODY);
        if (expect != rd_u32(raw + CLOCK_BLOB_V1_BODY)) {
            return false;
        }
        decode_v1_body(raw, out);
        return true;
    }

    if (version == 0 && len == 8) {
        uint32_t expect = clock_crc32(raw, 4);
        if (expect != rd_u32(raw + 4)) {
            return false;
        }
        clock_config_defaults(out);
        uint16_t refresh = rd_u16(raw + 2);
        if (clock_refresh_allowed(refresh)) {
            out->refresh_minutes = refresh;
        }
        if (migrated != NULL) {
            *migrated = true;
        }
        return true;
    }

    return false;
}

size_t clock_config_encode(const clock_config_t *cfg, uint8_t *out, size_t cap)
{
    if (cfg == NULL || out == NULL || cap < CLOCK_BLOB_V1_SIZE) {
        return 0;
    }
    memset(out, 0, CLOCK_BLOB_V1_SIZE);
    wr_u16(out, CLOCK_SCHEMA_VERSION);
    wr_u16(out + 2, cfg->refresh_minutes);
    wr_u16(out + 4, cfg->dim_start_min);
    wr_u16(out + 6, cfg->dim_end_min);
    out[8] = cfg->brightness_day;
    out[9] = cfg->brightness_night;
    out[10] = cfg->forecast_hour;
    out[11] = cfg->font_clock;
    out[12] = cfg->font_temp;
    out[13] = cfg->font_label;
    memcpy(out + 14, cfg->template_name, CLOCK_TEMPLATE_NAME_LEN - 1);
    wr_u32(out + CLOCK_BLOB_V1_BODY, clock_crc32(out, CLOCK_BLOB_V1_BODY));
    return CLOCK_BLOB_V1_SIZE;
}

int clock_round_half_away(float value)
{
    double v = (double)value;
    if (v >= 0.0) {
        return (int)floor(v + 0.5);
    }
    return (int)ceil(v - 0.5);
}

int clock_temp_band(int rounded)
{
    if (rounded < -5) {
        return 0;
    }
    if (rounded < 5) {
        return 1;
    }
    if (rounded < 15) {
        return 2;
    }
    if (rounded <= 25) {
        return 3;
    }
    return 4;
}

int clock_temp_color(int actual_rounded, int feels_rounded)
{
    if (feels_rounded < actual_rounded) {
        return -1;
    }
    if (feels_rounded > actual_rounded) {
        return 1;
    }
    return 0;
}

static int days_in_month(int year, int month)
{
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        return 29;
    }
    if (month < 1 || month > 12) {
        return 30;
    }
    return days[month];
}

static void next_day(int *year, int *month, int *day)
{
    (*day)++;
    if (*day > days_in_month(*year, *month)) {
        *day = 1;
        (*month)++;
        if (*month > 12) {
            *month = 1;
            (*year)++;
        }
    }
}

void forecast_select(int year, int month, int day, int hour, int minute, int forecast_hour, forecast_slot_t *out)
{
    (void)minute;
    if (hour >= forecast_hour) {
        next_day(&year, &month, &day);
        out->kind = FORECAST_TOMORROW;
        out->hour = 8;
    } else {
        out->kind = FORECAST_TODAY;
        out->hour = forecast_hour;
    }
    out->year = year;
    out->month = month;
    out->day = day;
}

bool brightness_is_dim(int minute_of_day, int start_min, int end_min)
{
    if (start_min == end_min || minute_of_day < 0 || minute_of_day > 1439) {
        return false;
    }
    if (start_min < end_min) {
        return minute_of_day >= start_min && minute_of_day < end_min;
    }
    return minute_of_day >= start_min || minute_of_day < end_min;
}

wmo_icon_t wmo_icon(int code)
{
    switch (code) {
    case 0:
        return WMO_SUN;
    case 1:
    case 2:
        return WMO_PARTLY;
    case 3:
        return WMO_CLOUD;
    case 45:
    case 48:
        return WMO_FOG;
    case 51:
    case 53:
    case 55:
    case 61:
    case 63:
    case 65:
    case 80:
    case 81:
    case 82:
        return WMO_RAIN;
    case 56:
    case 57:
    case 66:
    case 67:
        return WMO_FREEZE;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
        return WMO_SNOW;
    case 95:
    case 96:
    case 99:
        return WMO_STORM;
    default:
        return WMO_UNKNOWN;
    }
}

const char *clock_logic_self_test(void)
{
    if (clock_round_half_away(2.5f) != 3) {
        return "round 2.5";
    }
    if (clock_round_half_away(-2.5f) != -3) {
        return "round -2.5";
    }
    if (clock_round_half_away(0.4f) != 0 || clock_round_half_away(-0.4f) != 0) {
        return "round near zero";
    }
    if (clock_round_half_away(-5.4f) != -5 || clock_temp_band(-5) != 1) {
        return "band -5";
    }
    if (clock_round_half_away(-5.5f) != -6 || clock_temp_band(-6) != 0) {
        return "band -6";
    }
    if (clock_temp_band(4) != 1 || clock_temp_band(5) != 2 || clock_temp_band(14) != 2) {
        return "band mid";
    }
    if (clock_temp_band(15) != 3 || clock_round_half_away(25.4f) != 25 || clock_temp_band(25) != 3) {
        return "band 25";
    }
    if (clock_round_half_away(25.5f) != 26 || clock_temp_band(26) != 4) {
        return "band 26";
    }
    if (clock_temp_color(3, 1) != -1 || clock_temp_color(3, 5) != 1 || clock_temp_color(3, 3) != 0) {
        return "color";
    }

    forecast_slot_t slot;
    forecast_select(2026, 11, 8, 14, 59, 15, &slot);
    if (slot.kind != FORECAST_TODAY || slot.hour != 15 || slot.day != 8) {
        return "forecast today";
    }
    forecast_select(2026, 11, 8, 15, 0, 15, &slot);
    if (slot.kind != FORECAST_TOMORROW || slot.hour != 8 || slot.day != 9) {
        return "forecast tomorrow";
    }
    forecast_select(2026, 11, 8, 0, 0, 15, &slot);
    if (slot.kind != FORECAST_TODAY || slot.hour != 15 || slot.day != 8) {
        return "forecast midnight";
    }
    forecast_select(2024, 2, 28, 16, 0, 15, &slot);
    if (slot.day != 29 || slot.month != 2 || slot.kind != FORECAST_TOMORROW) {
        return "forecast leap";
    }
    forecast_select(2025, 2, 28, 15, 0, 15, &slot);
    if (slot.day != 1 || slot.month != 3 || slot.year != 2025) {
        return "forecast march";
    }
    forecast_select(2025, 12, 31, 20, 0, 15, &slot);
    if (slot.year != 2026 || slot.month != 1 || slot.day != 1 || slot.hour != 8) {
        return "forecast new year";
    }

    if (!brightness_is_dim(23 * 60, 20 * 60, 6 * 60) || !brightness_is_dim(3 * 60, 20 * 60, 6 * 60)) {
        return "dim night";
    }
    if (brightness_is_dim(12 * 60, 20 * 60, 6 * 60) || brightness_is_dim(6 * 60, 20 * 60, 6 * 60)) {
        return "dim day";
    }
    if (!brightness_is_dim(20 * 60, 20 * 60, 6 * 60) || brightness_is_dim(20 * 60, 20 * 60, 20 * 60)) {
        return "dim edge";
    }

    if (wmo_icon(0) != WMO_SUN || wmo_icon(2) != WMO_PARTLY || wmo_icon(3) != WMO_CLOUD) {
        return "wmo sky";
    }
    if (wmo_icon(45) != WMO_FOG || wmo_icon(63) != WMO_RAIN || wmo_icon(66) != WMO_FREEZE) {
        return "wmo wet";
    }
    if (wmo_icon(71) != WMO_SNOW || wmo_icon(95) != WMO_STORM || wmo_icon(100) != WMO_UNKNOWN ||
        wmo_icon(-1) != WMO_UNKNOWN) {
        return "wmo other";
    }

    const char *message = NULL;
    clock_config_t cfg;
    clock_config_defaults(&cfg);
    if (!clock_config_validate(&cfg, &message)) {
        return "validate default";
    }
    cfg.dim_end_min = cfg.dim_start_min;
    if (clock_config_validate(&cfg, &message)) {
        return "validate dim";
    }
    cfg.dim_end_min = CLOCK_DIM_END_DEFAULT;
    cfg.forecast_hour = 99;
    if (!clock_config_repair(&cfg) || cfg.forecast_hour != CLOCK_FORECAST_HOUR_DEFAULT ||
        cfg.refresh_minutes != CLOCK_REFRESH_DEFAULT) {
        return "repair";
    }

    uint8_t blob[128];
    clock_config_defaults(&cfg);
    memcpy(cfg.template_name, "Kraina-lodu", 13);
    size_t n = clock_config_encode(&cfg, blob, sizeof(blob));
    clock_config_t back;
    bool migrated = true;
    if (!clock_config_decode(blob, n, &back, &migrated) || migrated || clock_name_cmp(back.template_name, "kraina-lodu") != 0) {
        return "blob";
    }
    blob[10] = 3;
    if (clock_config_decode(blob, n, &back, &migrated)) {
        return "crc";
    }
    n = clock_config_encode(&cfg, blob, sizeof(blob));
    blob[10] = 99;
    wr_u32(blob + CLOCK_BLOB_V1_BODY, clock_crc32(blob, CLOCK_BLOB_V1_BODY));
    if (!clock_config_decode(blob, n, &back, &migrated) || back.forecast_hour != 99) {
        return "range kept";
    }
    if (!clock_config_repair(&back) || back.forecast_hour != CLOCK_FORECAST_HOUR_DEFAULT ||
        clock_name_cmp(back.template_name, "Kraina-lodu") != 0) {
        return "range repair";
    }

    uint8_t old[8];
    wr_u16(old, 0);
    wr_u16(old + 2, 30);
    wr_u32(old + 4, clock_crc32(old, 4));
    if (!clock_config_decode(old, sizeof(old), &back, &migrated) || !migrated || back.refresh_minutes != 30 ||
        back.forecast_hour != CLOCK_FORECAST_HOUR_DEFAULT) {
        return "migrate";
    }
    if (clock_name_cmp("Alfa", "alfa") != 0 || !clock_template_name_ok("Zima") || clock_template_name_ok("../x")) {
        return "name";
    }
    return NULL;
}
