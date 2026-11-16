#include "http_panel.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_state.h"
#include "clock_logic.h"
#include "config_store.h"
#include "log_store.h"
#include "sdcard.h"
#include "secrets.h"
#include "time_sync.h"
#include "weather.h"

static const char *TAG = "http";
static httpd_handle_t s_server;
static char s_token[33];
static int64_t s_token_until;
static int s_failures;

static bool constant_equal(const char *left, const char *right)
{
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    size_t n = left_len > right_len ? left_len : right_len;
    unsigned diff = (unsigned)(left_len ^ right_len);
    for (size_t i = 0; i < n; i++) {
        unsigned a = i < left_len ? (unsigned char)left[i] : 0;
        unsigned b = i < right_len ? (unsigned char)right[i] : 0;
        diff |= a ^ b;
    }
    return diff == 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *text)
{
    char *out = text;
    for (char *in = text; *in != '\0'; in++) {
        if (*in == '+') {
            *out++ = ' ';
        } else if (*in == '%' && hex_value(in[1]) >= 0 && hex_value(in[2]) >= 0) {
            *out++ = (char)((hex_value(in[1]) << 4) | hex_value(in[2]));
            in += 2;
        } else {
            *out++ = *in;
        }
    }
    *out = '\0';
}

typedef struct {
    char key[32];
    char value[CLOCK_TEMPLATE_NAME_LEN];
} form_field_t;

static int parse_form(char *body, form_field_t *fields, int capacity)
{
    int count = 0;
    char *cursor = body;
    while (cursor != NULL && *cursor != '\0' && count < capacity) {
        char *amp = strchr(cursor, '&');
        if (amp != NULL) {
            *amp = '\0';
        }
        char *eq = strchr(cursor, '=');
        if (eq != NULL) {
            *eq = '\0';
            url_decode(cursor);
            url_decode(eq + 1);
            snprintf(fields[count].key, sizeof(fields[count].key), "%s", cursor);
            snprintf(fields[count].value, sizeof(fields[count].value), "%s", eq + 1);
            count++;
        }
        cursor = amp != NULL ? amp + 1 : NULL;
    }
    return count;
}

static const char *field_value(const form_field_t *fields, int count, const char *key)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(fields[i].key, key) == 0) {
            return fields[i].value;
        }
    }
    return "";
}

static void issue_token(void)
{
    for (int i = 0; i < 16; i++) {
        snprintf(s_token + (i * 2), 3, "%02x", (unsigned)(esp_random() & 0xFF));
    }
    s_token_until = esp_timer_get_time() + 30LL * 60 * 1000000;
}

static bool session_ok(httpd_req_t *req)
{
    if (s_token[0] == '\0' || esp_timer_get_time() > s_token_until) {
        return false;
    }
    char cookie[160];
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) {
        return false;
    }
    char *mark = strstr(cookie, "sesja=");
    if (mark == NULL) {
        return false;
    }
    mark += 6;
    char given[40];
    snprintf(given, sizeof(given), "%s", mark);
    char *end = strpbrk(given, "; ");
    if (end != NULL) {
        *end = '\0';
    }
    return constant_equal(given, s_token);
}

static void send_html(httpd_req_t *req, const char *body)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static void send_login(httpd_req_t *req, const char *error)
{
    char page[1024];
    snprintf(page, sizeof(page),
             "<!DOCTYPE html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
             "content=\"width=device-width\"><title>Zegar</title></head><body style=\"font-family:sans-serif;"
             "background:#1c1c1c;color:#fff;padding:24px\"><h1>Zegar pogodowy</h1><p>%s</p>"
             "<form method=\"post\" action=\"/configuration/login\">Hasło<br>"
             "<input type=\"password\" name=\"haslo\" autofocus><br><br>"
             "<button type=\"submit\">Zaloguj</button></form></body></html>",
             error != NULL ? error : "");
    if (error != NULL) {
        httpd_resp_set_status(req, "401 Unauthorized");
    }
    send_html(req, page);
}

static int selected_int(const char *value, int fallback)
{
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    return (int)strtol(value, NULL, 10);
}

static void append_page(httpd_req_t *req, const char *error)
{
    app_model_t model;
    app_state_copy(&model);
    char *page = malloc(12288);
    if (page == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Brak pamieci");
        return;
    }
    char when[32] = "brak";
    if (model.weather_at != 0 && model.time_synced) {
        struct tm local;
        localtime_r(&model.weather_at, &local);
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
    }
    char names[32][32];
    int logs = model.logs_on ? log_list(names, 32) : 0;
    int used = snprintf(page, 12288,
                        "<!DOCTYPE html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
                        "content=\"width=device-width\"><title>Konfiguracja</title></head>"
                        "<body style=\"font-family:sans-serif;background:#1c1c1c;color:#fff;padding:16px\">"
                        "<h1>Konfiguracja</h1><p style=\"color:#ffb000\">%s</p>"
                        "<form method=\"post\" action=\"/configuration/save\">"
                        "<p>Szablon<br><select name=\"szablon\">",
                        error != NULL ? error : model.config_error);
    if (model.template_count == 0) {
        used += snprintf(page + used, 12288 - (size_t)used, "<option value=\"\">Awaryjny</option>");
    }
    for (int i = 0; i < model.template_count && used < 11000; i++) {
        used += snprintf(page + used, 12288 - (size_t)used, "<option value=\"%s\"%s>%s</option>", model.templates[i],
                         clock_name_cmp(model.templates[i], model.cfg.template_name) == 0 ? " selected" : "",
                         model.templates[i]);
    }
    used += snprintf(page + used, 12288 - (size_t)used,
                     "</select></p><p>Odświeżanie<br><select name=\"interwal\">");
    const int intervals[] = {30, 60, 120, 180, 360};
    const char *labels[] = {"30 min", "1 godz.", "2 godz.", "3 godz.", "6 godz."};
    for (int i = 0; i < 5; i++) {
        used += snprintf(page + used, 12288 - (size_t)used, "<option value=\"%d\"%s>%s</option>", intervals[i],
                         intervals[i] == model.cfg.refresh_minutes ? " selected" : "", labels[i]);
    }
    used += snprintf(page + used, 12288 - (size_t)used, "</select></p><p>Jasność dzienna<br><select name=\"jasnosc_dzien\">");
    for (int value = 10; value <= 100; value += 10) {
        used += snprintf(page + used, 12288 - (size_t)used, "<option value=\"%d\"%s>%d%%</option>", value,
                         value == model.cfg.brightness_day ? " selected" : "", value);
    }
    used += snprintf(page + used, 12288 - (size_t)used, "</select></p><p>Jasność zmniejszona<br><select name=\"jasnosc_noc\">");
    for (int value = 10; value <= 100; value += 10) {
        used += snprintf(page + used, 12288 - (size_t)used, "<option value=\"%d\"%s>%d%%</option>", value,
                         value == model.cfg.brightness_night ? " selected" : "", value);
    }
    used += snprintf(page + used, 12288 - (size_t)used,
                     "</select></p><p>Początek przyciemnienia (minuty od północy)<br>"
                     "<input name=\"przyciemnienie_start\" value=\"%u\"></p>"
                     "<p>Koniec przyciemnienia<br><input name=\"przyciemnienie_koniec\" value=\"%u\"></p>"
                     "<p>Godzina prognozy<br><input name=\"godzina_prognozy\" value=\"%u\"></p>"
                     "<p>Czcionka godziny (0 mały, 1 średni, 2 duży)<br><input name=\"czcionka_godzina\" value=\"%u\"></p>"
                     "<p>Czcionka temperatur<br><input name=\"czcionka_temp\" value=\"%u\"></p>"
                     "<p>Czcionka etykiet<br><input name=\"czcionka_etykieta\" value=\"%u\"></p>"
                     "<button type=\"submit\">Zapisz</button> <a href=\"/configuration\" style=\"color:#fff\">Anuluj</a></form>",
                     model.cfg.dim_start_min, model.cfg.dim_end_min, model.cfg.forecast_hour, model.cfg.font_clock,
                     model.cfg.font_temp, model.cfg.font_label);
    used += snprintf(page + used, 12288 - (size_t)used,
                     "<h2>Diagnostyka</h2><p>SSID: %s<br>Adres: %s<br>Panel: http://%s/configuration<br>"
                     "Lokalizacja: %s<br>Teraz: %.1f / %.1f, kod %d, opady %.1f mm<br>"
                     "Ostatnia aktualizacja: %s<br>Open-Meteo: %s<br>Karta: %s<br>%s</p>",
                     APP_WIFI_SSID, model.wifi_up ? model.ip : "brak", model.wifi_up ? model.ip : "niedostępny",
                     CLOCK_LOCATION_NAME, model.cur_temp, model.cur_feels, model.cur_code, model.cur_precip, when,
                     model.weather_error[0] ? model.weather_error : (model.weather_ok ? "OK" : "brak danych"),
                     model.sd_ok ? "działa" : (model.sd_error[0] ? model.sd_error : "niedostępna"), model.template_note);
    if (!model.logs_on) {
        used += snprintf(page + used, 12288 - (size_t)used, "<h2>Logi</h2><p>Logowanie do plików jest wyłączone, bo karta TF nie działa.</p>");
    } else {
        used += snprintf(page + used, 12288 - (size_t)used, "<h2>Logi</h2><ul>");
        for (int i = 0; i < logs && used < 11000; i++) {
            used += snprintf(page + used, 12288 - (size_t)used, "<li><a style=\"color:#9cf\" href=\"/configuration/logs/%s\">%s</a></li>",
                             names[i], names[i]);
        }
        used += snprintf(page + used, 12288 - (size_t)used, "</ul>");
    }
    snprintf(page + used, 12288 - (size_t)used, "</body></html>");
    send_html(req, page);
    free(page);
}

static bool read_body(httpd_req_t *req, char *body, size_t cap)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= cap) {
        return false;
    }
    size_t got = 0;
    while (got < (size_t)req->content_len) {
        int n = httpd_req_recv(req, body + got, (size_t)req->content_len - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        got += (size_t)n;
    }
    body[got] = '\0';
    return true;
}

static esp_err_t configuration_get(httpd_req_t *req)
{
    if (!session_ok(req)) {
        send_login(req, NULL);
        return ESP_OK;
    }
    append_page(req, NULL);
    return ESP_OK;
}

static esp_err_t login_post(httpd_req_t *req)
{
    char body[256];
    if (!read_body(req, body, sizeof(body))) {
        send_login(req, "Nie udało się odczytać formularza");
        return ESP_OK;
    }
    form_field_t fields[4];
    int field_count = parse_form(body, fields, 4);
    const char *password = field_value(fields, field_count, "haslo");
    if (!constant_equal(password, APP_PANEL_PASSWORD)) {
        int shift = s_failures > 4 ? 4 : s_failures;
        if (s_failures < 8) {
            s_failures++;
        }
        log_event(LOG_WARN, "Panel: nieudane logowanie");
        vTaskDelay(pdMS_TO_TICKS(1000 << shift));
        send_login(req, "Nieprawidłowe hasło");
        return ESP_OK;
    }
    s_failures = 0;
    issue_token();
    char cookie[80];
    snprintf(cookie, sizeof(cookie), "sesja=%s; HttpOnly; Path=/; Max-Age=1800", s_token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/configuration");
    httpd_resp_send(req, "", 0);
    log_event(LOG_INFO, "Panel: logowanie");
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    if (!session_ok(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Wymagane haslo", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    char body[512];
    if (!read_body(req, body, sizeof(body))) {
        append_page(req, "Nie udało się odczytać formularza");
        return ESP_OK;
    }
    form_field_t fields[16];
    int field_count = parse_form(body, fields, 16);
    app_model_t model;
    app_state_copy(&model);
    clock_config_t cfg = model.cfg;
    cfg.refresh_minutes = (uint16_t)selected_int(field_value(fields, field_count, "interwal"), cfg.refresh_minutes);
    cfg.brightness_day = (uint8_t)selected_int(field_value(fields, field_count, "jasnosc_dzien"), cfg.brightness_day);
    cfg.brightness_night = (uint8_t)selected_int(field_value(fields, field_count, "jasnosc_noc"), cfg.brightness_night);
    cfg.dim_start_min = (uint16_t)selected_int(field_value(fields, field_count, "przyciemnienie_start"), cfg.dim_start_min);
    cfg.dim_end_min = (uint16_t)selected_int(field_value(fields, field_count, "przyciemnienie_koniec"), cfg.dim_end_min);
    cfg.forecast_hour = (uint8_t)selected_int(field_value(fields, field_count, "godzina_prognozy"), cfg.forecast_hour);
    cfg.font_clock = (uint8_t)selected_int(field_value(fields, field_count, "czcionka_godzina"), cfg.font_clock);
    cfg.font_temp = (uint8_t)selected_int(field_value(fields, field_count, "czcionka_temp"), cfg.font_temp);
    cfg.font_label = (uint8_t)selected_int(field_value(fields, field_count, "czcionka_etykieta"), cfg.font_label);
    const char *name = field_value(fields, field_count, "szablon");
    snprintf(cfg.template_name, sizeof(cfg.template_name), "%s", name);
    bool known = cfg.template_name[0] == '\0' && model.template_count == 0;
    for (int i = 0; i < model.template_count; i++) {
        if (clock_name_cmp(model.templates[i], cfg.template_name) == 0) {
            known = true;
        }
    }
    const char *message = NULL;
    if (!known || !clock_config_validate(&cfg, &message) || !config_store_save(&cfg)) {
        append_page(req, message != NULL ? message : "Nie można zapisać");
        return ESP_OK;
    }
    log_event(LOG_INFO, "Panel: zapis konfiguracji");
    append_page(req, "Zapisano");
    return ESP_OK;
}

static esp_err_t log_get(httpd_req_t *req)
{
    if (!session_ok(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Wymagane haslo", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    const char *uri = req->uri;
    const char *slash = strrchr(uri, '/');
    const char *raw_name = slash != NULL ? slash + 1 : uri;
    char name[32];
    size_t name_len = strlen(raw_name);
    if (name_len >= sizeof(name) || !log_safe_name(raw_name)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nie znaleziono");
        return ESP_OK;
    }
    memcpy(name, raw_name, name_len + 1);
    app_model_t model;
    app_state_copy(&model);
    if (!model.logs_on) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Logi wylaczone");
        return ESP_OK;
    }
    char path[80];
    snprintf(path, sizeof(path), "/sdcard/logs/%s", name);
    sdcard_lock();
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        sdcard_unlock();
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nie znaleziono");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    char disposition[80];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", name);
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);
    char buffer[512];
    size_t sent = 0;
    while (sent < 1024 * 1024) {
        size_t n = fread(buffer, 1, sizeof(buffer), file);
        if (n == 0) {
            break;
        }
        if (httpd_resp_send_chunk(req, buffer, n) != ESP_OK) {
            break;
        }
        sent += n;
    }
    fclose(file);
    sdcard_unlock();
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

void http_panel_start(void)
{
    if (s_server != NULL) {
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 10240;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.max_open_sockets = 4;
    if (httpd_start(&s_server, &config) != ESP_OK) {
        s_server = NULL;
        ESP_LOGE(TAG, "Nie można uruchomić panelu");
        return;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/configuration", .method = HTTP_GET, .handler = configuration_get},
        {.uri = "/configuration/login", .method = HTTP_POST, .handler = login_post},
        {.uri = "/configuration/save", .method = HTTP_POST, .handler = save_post},
        {.uri = "/configuration/logs/*", .method = HTTP_GET, .handler = log_get},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(s_server, &routes[i]);
    }
    log_event(LOG_INFO, "Panel WWW uruchomiony");
}
