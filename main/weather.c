#include "weather.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_state.h"
#include "clock_logic.h"
#include "log_store.h"
#include "time_sync.h"

static const char *TAG = "weather";
#define MAX_HOURS 64

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    float temp;
    float feels;
    float precip;
    int prob;
} hour_slot_t;

static hour_slot_t s_hours[MAX_HOURS];
static int s_hour_count;
static SemaphoreHandle_t s_lock;
static volatile bool s_force;
static bool s_expiry_logged;

void weather_request_fetch(void)
{
    s_force = true;
}

bool weather_lookup(int year, int month, int day, int hour, float *temp, float *feels, float *precip, int *prob)
{
    if (s_lock == NULL) {
        return false;
    }
    bool found = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < s_hour_count; i++) {
        if (s_hours[i].year == year && s_hours[i].month == month && s_hours[i].day == day && s_hours[i].hour == hour) {
            if (temp != NULL) {
                *temp = s_hours[i].temp;
            }
            if (feels != NULL) {
                *feels = s_hours[i].feels;
            }
            if (precip != NULL) {
                *precip = s_hours[i].precip;
            }
            if (prob != NULL) {
                *prob = s_hours[i].prob;
            }
            found = true;
            break;
        }
    }
    xSemaphoreGive(s_lock);
    return found;
}

static bool parse_stamp(const char *text, int *year, int *month, int *day, int *hour)
{
    int minute = 0;
    if (text == NULL || sscanf(text, "%d-%d-%dT%d:%d", year, month, day, hour, &minute) != 5) {
        return false;
    }
    return *month >= 1 && *month <= 12 && *day >= 1 && *day <= 31 && *hour >= 0 && *hour <= 23;
}

static bool number_of(cJSON *object, const char *key, double *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

static void remember_error(const char *text)
{
    app_state_lock();
    app_model_t *model = app_state_edit();
    snprintf(model->weather_error, sizeof(model->weather_error), "%s", text);
    app_state_bump();
    app_state_unlock();
}

static bool parse_payload(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        remember_error("Niepoprawna odpowiedź Open-Meteo");
        log_event(LOG_WARN, "Open-Meteo: niepoprawny JSON");
        return false;
    }

    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    double cur_temp = 0;
    double cur_feels = 0;
    double cur_precip = 0;
    double cur_code = 0;
    bool ok = current != NULL && hourly != NULL && number_of(current, "temperature_2m", &cur_temp) &&
              number_of(current, "apparent_temperature", &cur_feels) &&
              number_of(current, "precipitation", &cur_precip) && number_of(current, "weather_code", &cur_code);

    cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    cJSON *temps = cJSON_GetObjectItemCaseSensitive(hourly, "temperature_2m");
    cJSON *feels = cJSON_GetObjectItemCaseSensitive(hourly, "apparent_temperature");
    cJSON *precips = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation");
    cJSON *probs = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");
    int count = (times != NULL) ? cJSON_GetArraySize(times) : 0;
    if (!ok || !cJSON_IsArray(times) || !cJSON_IsArray(temps) || !cJSON_IsArray(feels) || !cJSON_IsArray(precips) ||
        !cJSON_IsArray(probs) || count <= 0 || cJSON_GetArraySize(temps) != count ||
        cJSON_GetArraySize(feels) != count || cJSON_GetArraySize(precips) != count ||
        cJSON_GetArraySize(probs) != count) {
        cJSON_Delete(root);
        remember_error("Brak wymaganych danych pogody");
        log_event(LOG_WARN, "Open-Meteo: brak wymaganych pól");
        return false;
    }

    hour_slot_t parsed[MAX_HOURS];
    int stored = 0;
    for (int i = 0; i < count && stored < MAX_HOURS; i++) {
        cJSON *stamp = cJSON_GetArrayItem(times, i);
        cJSON *temp = cJSON_GetArrayItem(temps, i);
        cJSON *feel = cJSON_GetArrayItem(feels, i);
        cJSON *precip = cJSON_GetArrayItem(precips, i);
        cJSON *prob = cJSON_GetArrayItem(probs, i);
        int year, month, day, hour;
        if (!cJSON_IsString(stamp) || !parse_stamp(stamp->valuestring, &year, &month, &day, &hour) ||
            !cJSON_IsNumber(temp) || !cJSON_IsNumber(feel) || !cJSON_IsNumber(precip) || !cJSON_IsNumber(prob)) {
            cJSON_Delete(root);
            remember_error("Niepoprawna prognoza godzinowa");
            log_event(LOG_WARN, "Open-Meteo: niepoprawna godzina prognozy");
            return false;
        }
        parsed[stored].year = year;
        parsed[stored].month = month;
        parsed[stored].day = day;
        parsed[stored].hour = hour;
        parsed[stored].temp = (float)temp->valuedouble;
        parsed[stored].feels = (float)feel->valuedouble;
        parsed[stored].precip = (float)precip->valuedouble;
        parsed[stored].prob = (int)prob->valuedouble;
        stored++;
    }
    if (stored < count) {
        cJSON_Delete(root);
        remember_error("Prognoza jest za długa");
        log_event(LOG_WARN, "Open-Meteo: zbyt wiele godzin");
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_hours, parsed, sizeof(hour_slot_t) * (size_t)stored);
    s_hour_count = stored;
    xSemaphoreGive(s_lock);

    app_state_lock();
    app_model_t *model = app_state_edit();
    model->weather_ok = true;
    model->weather_at = time(NULL);
    model->cur_temp = (float)cur_temp;
    model->cur_feels = (float)cur_feels;
    model->cur_precip = (float)cur_precip;
    model->cur_code = (int)cur_code;
    model->weather_error[0] = '\0';
    app_state_bump();
    app_state_unlock();
    s_expiry_logged = false;
    cJSON_Delete(root);
    log_event(LOG_INFO, "Open-Meteo: pobrano, kod %d", (int)cur_code);
    return true;
}

static bool fetch_once(void)
{
    char url[384];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,apparent_temperature,weather_code,precipitation"
             "&hourly=temperature_2m,apparent_temperature,precipitation_probability,precipitation"
             "&timezone=Europe%%2FWarsaw&forecast_days=2",
             CLOCK_LATITUDE_STR, CLOCK_LONGITUDE_STR);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        remember_error("Brak pamięci na pobranie pogody");
        log_event(LOG_ERROR, "Open-Meteo: brak pamięci klienta");
        return false;
    }
    esp_http_client_set_header(client, "User-Agent", "children-weather-clock");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        remember_error("Brak połączenia z Open-Meteo");
        log_event(LOG_WARN, "Open-Meteo: połączenie nieudane");
        return false;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    char *body = heap_caps_malloc(48 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (body == NULL) {
        body = malloc(48 * 1024);
    }
    if (body == NULL) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        remember_error("Brak pamięci na odpowiedź pogody");
        log_event(LOG_ERROR, "Open-Meteo: brak bufora");
        return false;
    }
    int total = 0;
    while (total < 48 * 1024 - 1) {
        int n = esp_http_client_read(client, body + total, 48 * 1024 - 1 - total);
        if (n < 0) {
            total = -1;
            break;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (total < 0 || status != 200) {
        free(body);
        remember_error("Błąd odpowiedzi Open-Meteo");
        log_event(LOG_WARN, "Open-Meteo: status %d", status);
        return false;
    }
    body[total] = '\0';
    bool ok = parse_payload(body);
    free(body);
    return ok;
}

static bool wifi_is_up(void)
{
    app_model_t model;
    app_state_copy(&model);
    return model.wifi_up;
}

static int refresh_seconds(void)
{
    app_model_t model;
    app_state_copy(&model);
    int minutes = model.cfg.refresh_minutes;
    if (!clock_refresh_allowed(minutes)) {
        minutes = CLOCK_REFRESH_DEFAULT;
    }
    return minutes * 60;
}

static bool fetch_with_retries(void)
{
    static const int waits[] = {60, 300, 900};
    if (fetch_once()) {
        return true;
    }
    for (int attempt = 0; attempt < CLOCK_RETRY_COUNT; attempt++) {
        for (int second = 0; second < waits[attempt]; second++) {
            if (s_force || !wifi_is_up() || !time_is_synced()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (!wifi_is_up() || !time_is_synced()) {
            return false;
        }
        s_force = false;
        log_event(LOG_INFO, "Open-Meteo: ponowienie %d", attempt + 1);
        if (fetch_once()) {
            return true;
        }
    }
    log_event(LOG_WARN, "Open-Meteo: wyczerpano ponowienia");
    return false;
}

static void note_expiry(void)
{
    app_model_t model;
    app_state_copy(&model);
    if (!model.weather_ok || !time_is_synced()) {
        return;
    }
    time_t now = time(NULL);
    int limit = refresh_seconds() * 4;
    if ((now - model.weather_at) >= limit) {
        if (!s_expiry_logged) {
            s_expiry_logged = true;
            log_event(LOG_WARN, "Pogoda: dane wygasły");
        }
    }
}

static void weather_task(void *arg)
{
    (void)arg;
    time_t next = 0;
    while (true) {
        if (!time_is_synced() || !wifi_is_up()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        time_t now = time(NULL);
        if (s_force || next == 0 || now >= next) {
            s_force = false;
            fetch_with_retries();
            next = time(NULL) + refresh_seconds();
        }
        note_expiry();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void weather_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (xTaskCreateWithCaps(weather_task, "weather", 20480, NULL, 3, NULL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        remember_error("Brak pamięci na pobranie pogody");
        log_event(LOG_ERROR, "Open-Meteo: brak pamięci zadania");
    }
}
