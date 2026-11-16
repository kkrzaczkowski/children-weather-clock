#include "ui_config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "clock_logic.h"
#include "config_store.h"
#include "secrets.h"

extern const lv_font_t font_label_20;

static lv_obj_t *s_root;
static lv_obj_t *s_message;
static lv_obj_t *s_diag;
static lv_obj_t *s_template;
static lv_obj_t *s_refresh;
static lv_obj_t *s_day;
static lv_obj_t *s_night;
static lv_obj_t *s_start_hour;
static lv_obj_t *s_start_min;
static lv_obj_t *s_end_hour;
static lv_obj_t *s_end_min;
static lv_obj_t *s_forecast;
static lv_obj_t *s_font_clock;
static lv_obj_t *s_font_temp;
static lv_obj_t *s_font_label;
static uint32_t s_activity;
static char s_hours[96];
static char s_minutes[220];
static char s_forecast_opts[80];
static char s_templates[CLOCK_TEMPLATE_MAX * CLOCK_TEMPLATE_NAME_LEN];
static bool s_opts_ready;

static const int s_refresh_values[] = {30, 60, 120, 180, 360};

static void ensure_opts(void)
{
    if (s_opts_ready) {
        return;
    }
    s_hours[0] = '\0';
    s_minutes[0] = '\0';
    s_forecast_opts[0] = '\0';
    for (int i = 0; i < 24; i++) {
        char piece[8];
        snprintf(piece, sizeof(piece), "%s%02d", i == 0 ? "" : "\n", i);
        strncat(s_hours, piece, sizeof(s_hours) - strlen(s_hours) - 1);
    }
    for (int i = 0; i < 60; i++) {
        char piece[8];
        snprintf(piece, sizeof(piece), "%s%02d", i == 0 ? "" : "\n", i);
        strncat(s_minutes, piece, sizeof(s_minutes) - strlen(s_minutes) - 1);
    }
    for (int hour = CLOCK_FORECAST_HOUR_MIN; hour <= CLOCK_FORECAST_HOUR_MAX; hour++) {
        char piece[8];
        snprintf(piece, sizeof(piece), "%s%02d", hour == CLOCK_FORECAST_HOUR_MIN ? "" : "\n", hour);
        strncat(s_forecast_opts, piece, sizeof(s_forecast_opts) - strlen(s_forecast_opts) - 1);
    }
    s_opts_ready = true;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 760);
    lv_obj_set_style_text_font(label, &font_label_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    return label;
}

static lv_obj_t *add_dropdown(lv_obj_t *parent, const char *options)
{
    lv_obj_t *dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(dropdown, options);
    lv_obj_set_width(dropdown, 280);
    lv_obj_set_style_text_font(dropdown, &font_label_20, 0);
    return dropdown;
}

static int refresh_index(int minutes)
{
    for (int i = 0; i < 5; i++) {
        if (s_refresh_values[i] == minutes) {
            return i;
        }
    }
    return 2;
}

static void close_config(void)
{
    if (s_root != NULL) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
}

static void save_cb(lv_event_t *event)
{
    (void)event;
    app_model_t model;
    app_state_copy(&model);
    clock_config_t cfg = model.cfg;
    cfg.refresh_minutes = (uint16_t)s_refresh_values[lv_dropdown_get_selected(s_refresh)];
    cfg.brightness_day = (uint8_t)((lv_dropdown_get_selected(s_day) + 1) * 10);
    cfg.brightness_night = (uint8_t)((lv_dropdown_get_selected(s_night) + 1) * 10);
    cfg.dim_start_min = (uint16_t)(lv_dropdown_get_selected(s_start_hour) * 60 + lv_dropdown_get_selected(s_start_min));
    cfg.dim_end_min = (uint16_t)(lv_dropdown_get_selected(s_end_hour) * 60 + lv_dropdown_get_selected(s_end_min));
    cfg.forecast_hour = (uint8_t)(CLOCK_FORECAST_HOUR_MIN + lv_dropdown_get_selected(s_forecast));
    cfg.font_clock = (uint8_t)lv_dropdown_get_selected(s_font_clock);
    cfg.font_temp = (uint8_t)lv_dropdown_get_selected(s_font_temp);
    cfg.font_label = (uint8_t)lv_dropdown_get_selected(s_font_label);
    cfg.template_name[0] = '\0';
    if (model.template_count > 0) {
        int selected = (int)lv_dropdown_get_selected(s_template);
        if (selected >= 0 && selected < model.template_count) {
            snprintf(cfg.template_name, sizeof(cfg.template_name), "%s", model.templates[selected]);
        }
    }
    const char *message = NULL;
    if (!clock_config_validate(&cfg, &message)) {
        lv_label_set_text(s_message, message != NULL ? message : "Nie można zapisać");
        return;
    }
    if (!config_store_save(&cfg)) {
        lv_label_set_text(s_message, "Nie udało się zapisać");
        return;
    }
    close_config();
}

static void cancel_cb(lv_event_t *event)
{
    (void)event;
    close_config();
}

bool ui_config_is_open(void)
{
    return s_root != NULL;
}

void ui_config_open(void)
{
    if (s_root != NULL) {
        return;
    }
    ensure_opts();
    app_model_t model;
    app_state_copy(&model);
    s_activity = lv_tick_get();

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 800, 480);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 8, 0);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *scroll = lv_obj_create(s_root);
    lv_obj_set_size(scroll, 784, 400);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scroll, 6, 0);

    add_label(scroll, "Konfiguracja");
    s_message = add_label(scroll, model.config_error);
    lv_obj_set_style_text_color(s_message, lv_color_hex(0xFFB000), 0);

    add_label(scroll, "Szablon");
    s_templates[0] = '\0';
    if (model.template_count == 0) {
        snprintf(s_templates, sizeof(s_templates), "%s", "Awaryjny");
    } else {
        for (int i = 0; i < model.template_count; i++) {
            strncat(s_templates, i == 0 ? "" : "\n", sizeof(s_templates) - strlen(s_templates) - 1);
            strncat(s_templates, model.templates[i], sizeof(s_templates) - strlen(s_templates) - 1);
        }
    }
    s_template = add_dropdown(scroll, s_templates);
    int template_index = 0;
    for (int i = 0; i < model.template_count; i++) {
        if (clock_name_cmp(model.templates[i], model.cfg.template_name) == 0) {
            template_index = i;
        }
    }
    lv_dropdown_set_selected(s_template, (uint32_t)template_index);

    add_label(scroll, "Odświeżanie pogody");
    s_refresh = add_dropdown(scroll, "30 min\n1 godz.\n2 godz.\n3 godz.\n6 godz.");
    lv_dropdown_set_selected(s_refresh, (uint32_t)refresh_index(model.cfg.refresh_minutes));

    add_label(scroll, "Jasność dzienna");
    s_day = add_dropdown(scroll, "10%\n20%\n30%\n40%\n50%\n60%\n70%\n80%\n90%\n100%");
    lv_dropdown_set_selected(s_day, (uint32_t)((model.cfg.brightness_day / 10) - 1));
    add_label(scroll, "Jasność zmniejszona");
    s_night = add_dropdown(scroll, "10%\n20%\n30%\n40%\n50%\n60%\n70%\n80%\n90%\n100%");
    lv_dropdown_set_selected(s_night, (uint32_t)((model.cfg.brightness_night / 10) - 1));

    add_label(scroll, "Początek przyciemnienia");
    s_start_hour = add_dropdown(scroll, s_hours);
    s_start_min = add_dropdown(scroll, s_minutes);
    lv_dropdown_set_selected(s_start_hour, (uint32_t)(model.cfg.dim_start_min / 60));
    lv_dropdown_set_selected(s_start_min, (uint32_t)(model.cfg.dim_start_min % 60));
    add_label(scroll, "Koniec przyciemnienia");
    s_end_hour = add_dropdown(scroll, s_hours);
    s_end_min = add_dropdown(scroll, s_minutes);
    lv_dropdown_set_selected(s_end_hour, (uint32_t)(model.cfg.dim_end_min / 60));
    lv_dropdown_set_selected(s_end_min, (uint32_t)(model.cfg.dim_end_min % 60));

    add_label(scroll, "Godzina prognozy");
    s_forecast = add_dropdown(scroll, s_forecast_opts);
    lv_dropdown_set_selected(s_forecast, (uint32_t)(model.cfg.forecast_hour - CLOCK_FORECAST_HOUR_MIN));

    add_label(scroll, "Czcionka godziny");
    s_font_clock = add_dropdown(scroll, "Mały\nŚredni\nDuży");
    lv_dropdown_set_selected(s_font_clock, model.cfg.font_clock);
    add_label(scroll, "Czcionka temperatur");
    s_font_temp = add_dropdown(scroll, "Mały\nŚredni\nDuży");
    lv_dropdown_set_selected(s_font_temp, model.cfg.font_temp);
    add_label(scroll, "Czcionka etykiet");
    s_font_label = add_dropdown(scroll, "Mały\nŚredni\nDuży");
    lv_dropdown_set_selected(s_font_label, model.cfg.font_label);

    s_diag = add_label(scroll, "");
    lv_obj_set_style_text_color(s_diag, lv_color_hex(0xC8C8C8), 0);

    lv_obj_t *save = lv_button_create(s_root);
    lv_obj_set_pos(save, 24, 416);
    lv_obj_set_size(save, 180, 48);
    lv_obj_add_event_cb(save, save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(save);
    lv_label_set_text(save_label, "Zapisz");
    lv_obj_set_style_text_font(save_label, &font_label_20, 0);
    lv_obj_center(save_label);

    lv_obj_t *cancel = lv_button_create(s_root);
    lv_obj_set_pos(cancel, 220, 416);
    lv_obj_set_size(cancel, 180, 48);
    lv_obj_add_event_cb(cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "Anuluj");
    lv_obj_set_style_text_font(cancel_label, &font_label_20, 0);
    lv_obj_center(cancel_label);

    ui_config_tick(&model);
}

static void fmt_tenths(char *dst, size_t cap, float value)
{
    int scaled = (int)(value >= 0.0f ? value * 10.0f + 0.5f : value * 10.0f - 0.5f);
    int frac = scaled % 10;
    if (frac < 0) {
        frac = -frac;
    }
    snprintf(dst, cap, "%d.%d", scaled / 10, frac);
}

static void append_diag(char *dst, size_t cap, const char *line)
{
    size_t used = strlen(dst);
    if (used + 1 >= cap) {
        return;
    }
    size_t length = strlen(line);
    size_t room = cap - used - 1;
    if (length > room) {
        length = room;
    }
    memcpy(dst + used, line, length);
    dst[used + length] = '\0';
}

void ui_config_tick(const app_model_t *model)
{
    if (s_root == NULL) {
        return;
    }
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev != NULL && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
        s_activity = lv_tick_get();
    }
    if (lv_tick_get() - s_activity > CLOCK_CONFIG_IDLE_MS) {
        close_config();
        return;
    }

    char when[32] = "brak";
    if (model->weather_at != 0 && model->time_synced) {
        struct tm local;
        localtime_r(&model->weather_at, &local);
        strftime(when, sizeof(when), "%H:%M", &local);
    }
    char clock_text[16] = "00:00";
    const char *sync = "brak synchronizacji";
    if (model->time_synced) {
        time_t now = time(NULL);
        struct tm local;
        localtime_r(&now, &local);
        strftime(clock_text, sizeof(clock_text), "%H:%M", &local);
        sync = "zsynchronizowany";
    }
    char panel[64];
    if (model->wifi_up) {
        snprintf(panel, sizeof(panel), "http://%s/configuration", model->ip);
    } else {
        snprintf(panel, sizeof(panel), "%s", "panel niedostępny");
    }
    char temp[32];
    char feels[32];
    char precip[32];
    fmt_tenths(temp, sizeof(temp), model->cur_temp);
    fmt_tenths(feels, sizeof(feels), model->cur_feels);
    fmt_tenths(precip, sizeof(precip), model->cur_precip);
    char line[192];
    char text[1024];
    text[0] = '\0';
    snprintf(line, sizeof(line), "Czas: %s (%s)\n", clock_text, sync);
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Wi-Fi: %s\nSSID: %s\n", model->wifi_up ? "połączono" : "brak połączenia", APP_WIFI_SSID);
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Adres: %s\nPanel: %s\nLokalizacja: %s\n", model->wifi_up ? model->ip : "brak", panel,
             CLOCK_LOCATION_NAME);
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Teraz: %s / %s, kod %d, opady %s mm\n", temp, feels, model->cur_code, precip);
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Prognoza od: %s\n", when);
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Open-Meteo: %s\n",
             model->weather_error[0] != '\0' ? model->weather_error : (model->weather_ok ? "ostatnio OK" : "brak danych"));
    append_diag(text, sizeof(text), line);
    snprintf(line, sizeof(line), "Karta TF: %s\n", model->sd_ok ? "działa" : (model->sd_error[0] ? model->sd_error : "niedostępna"));
    append_diag(text, sizeof(text), line);
    if (model->template_note[0] != '\0') {
        snprintf(line, sizeof(line), "%s\n", model->template_note);
        append_diag(text, sizeof(text), line);
    }
    append_diag(text, sizeof(text), model->config_error);
    lv_label_set_text(s_diag, text);
}
