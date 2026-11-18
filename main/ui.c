#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "app_state.h"
#include "clock_logic.h"
#include "templates.h"
#include "time_sync.h"
#include "ui_config.h"
#include "weather.h"

extern const lv_font_t font_clock_59;
extern const lv_font_t font_clock_75;
extern const lv_font_t font_clock_86;
extern const lv_font_t font_temp_37;
extern const lv_font_t font_temp_45;
extern const lv_font_t font_label_20;
extern const lv_font_t font_label_28;
extern const lv_font_t font_label_36;

static const lv_font_t *s_clock_fonts[] = {&font_clock_59, &font_clock_75, &font_clock_86};
static const lv_font_t *s_temp_fonts[] = {&font_temp_37, &font_temp_45, &font_temp_45};
static const lv_font_t *s_label_fonts[] = {&font_label_20, &font_label_28, &font_label_36};

static lv_obj_t *s_left_img;
static lv_obj_t *s_right_img;
static lv_obj_t *s_clock;
static lv_obj_t *s_current_spans;
static lv_span_t *s_current_left;
static lv_span_t *s_current_right;
static lv_obj_t *s_forecast_spans;
static lv_span_t *s_forecast_left;
static lv_span_t *s_forecast_right;
static lv_obj_t *s_forecast_label;
static lv_obj_t *s_icon_box;
static lv_obj_t *s_status_box;
static lv_obj_t *s_alert;
static lv_obj_t *s_alert_label;
static char s_alert_text[208];
static lv_obj_t *s_dim;
static int s_gesture_count;
static int s_gesture_corner;
static uint32_t s_gesture_tick;
static int s_shown_icon = -1;
static int s_shown_status = -1;
static uint32_t s_shown_generation;
static char s_shown_name[CLOCK_TEMPLATE_NAME_LEN];
static int s_shown_left = -2;
static int s_shown_right = -2;

static void fmt_temp(int value, char *out, size_t cap)
{
    if (value < 0) {
        snprintf(out, cap, "−%d°", -value);
    } else {
        snprintf(out, cap, "%d°", value);
    }
}

static lv_color_t feels_color(int actual, int feels)
{
    int color = clock_temp_color(actual, feels);
    if (color < 0) {
        return lv_color_hex(0x2F6BFF);
    }
    if (color > 0) {
        return lv_color_hex(0xE02323);
    }
    return lv_color_white();
}

static void style_span(lv_span_t *span, const lv_font_t *font, lv_color_t color)
{
    lv_style_set_text_font(lv_span_get_style(span), font);
    lv_style_set_text_color(lv_span_get_style(span), color);
}

static void set_pair(lv_obj_t *group, lv_span_t *left, lv_span_t *right, int actual, int feels, const lv_font_t *font)
{
    char actual_text[16];
    char feels_text[16];
    char prefix[24];
    fmt_temp(actual, actual_text, sizeof(actual_text));
    fmt_temp(feels, feels_text, sizeof(feels_text));
    snprintf(prefix, sizeof(prefix), "%s / ", actual_text);
    style_span(left, font, lv_color_white());
    style_span(right, font, feels_color(actual, feels));
    lv_span_set_text(left, prefix);
    lv_span_set_text(right, feels_text);
    /* Expand mode keeps the size cached from the empty text until refresh. */
    lv_spangroup_refresh(group);
}

static lv_obj_t *dot(lv_obj_t *parent, lv_color_t color, int size)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static void build_weather_icon(wmo_icon_t kind)
{
    if ((int)kind == s_shown_icon) {
        return;
    }
    s_shown_icon = (int)kind;
    lv_obj_clean(s_icon_box);
    lv_obj_t *cloud = lv_obj_create(s_icon_box);
    lv_obj_set_size(cloud, 54, 28);
    lv_obj_align(cloud, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_radius(cloud, 14, 0);
    lv_obj_set_style_bg_color(cloud, lv_color_hex(0xD5D8DE), 0);
    lv_obj_set_style_border_width(cloud, 0, 0);
    lv_obj_remove_flag(cloud, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if (kind == WMO_SUN || kind == WMO_PARTLY) {
        dot(s_icon_box, lv_color_hex(0xFFD24A), kind == WMO_SUN ? 36 : 22);
        if (kind == WMO_SUN) {
            lv_obj_align(lv_obj_get_child(s_icon_box, 1), LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_flag(cloud, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_align(lv_obj_get_child(s_icon_box, 1), LV_ALIGN_CENTER, -16, -10);
        }
    } else if (kind == WMO_FOG) {
        lv_obj_set_style_bg_opa(cloud, LV_OPA_40, 0);
    } else if (kind == WMO_STORM) {
        dot(s_icon_box, lv_color_hex(0xFFD24A), 14);
        lv_obj_align(lv_obj_get_child(s_icon_box, 1), LV_ALIGN_CENTER, 0, 18);
    } else if (kind == WMO_UNKNOWN) {
        lv_obj_add_flag(cloud, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *mark = lv_label_create(s_icon_box);
        lv_label_set_text(mark, "?");
        lv_obj_set_style_text_font(mark, &font_label_36, 0);
        lv_obj_set_style_text_color(mark, lv_color_white(), 0);
        lv_obj_center(mark);
    }

    lv_color_t drop = lv_color_hex(0x4AA3FF);
    if (kind == WMO_SNOW) {
        drop = lv_color_white();
    } else if (kind == WMO_FREEZE) {
        drop = lv_color_hex(0x9EE7FF);
    }
    if (kind == WMO_RAIN || kind == WMO_SNOW || kind == WMO_FREEZE || kind == WMO_STORM) {
        for (int i = 0; i < 3; i++) {
            lv_obj_t *item = dot(s_icon_box, drop, kind == WMO_SNOW ? 8 : 6);
            lv_obj_align(item, LV_ALIGN_BOTTOM_MID, (i - 1) * 16, 0);
        }
    }
}

static void build_status(int kind)
{
    if (kind == s_shown_status) {
        return;
    }
    s_shown_status = kind;
    lv_obj_clean(s_status_box);
    if (kind < 0) {
        lv_obj_add_flag(s_status_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_HIDDEN);
    if (kind == 0) {
        for (int i = 0; i < 3; i++) {
            lv_obj_t *arc = lv_arc_create(s_status_box);
            lv_obj_set_size(arc, 16 + i * 14, 16 + i * 14);
            lv_arc_set_bg_angles(arc, 220, 320);
            lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
            lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN);
            lv_obj_remove_style(arc, NULL, LV_PART_INDICATOR);
            lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
            lv_obj_center(arc);
            lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        }
    } else {
        dot(s_status_box, lv_color_hex(0xD5D8DE), 28);
        lv_obj_t *slash = lv_obj_create(s_status_box);
        lv_obj_set_size(slash, 36, 4);
        lv_obj_set_style_bg_color(slash, lv_color_hex(0xE02323), 0);
        lv_obj_set_style_border_width(slash, 0, 0);
        lv_obj_set_style_transform_rotation(slash, 300, 0);
        lv_obj_center(slash);
        lv_obj_remove_flag(slash, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void append_alert(char *text, size_t cap, const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return;
    }
    size_t used = strlen(text);
    if (used > 0 && used + 1 < cap) {
        text[used++] = '\n';
        text[used] = '\0';
    }
    snprintf(text + used, cap - used, "%s", line);
}

static void update_alert(const app_model_t *model)
{
    char body[200];
    char shown[sizeof(s_alert_text)];
    body[0] = '\0';
    shown[0] = '\0';
    append_alert(body, sizeof(body), model->sd_error);
    append_alert(body, sizeof(body), model->wifi_error);
    if (body[0] != '\0') {
        snprintf(shown, sizeof(shown), "Uwaga\n%s", body);
    }
    if (strcmp(shown, s_alert_text) == 0) {
        return;
    }
    memcpy(s_alert_text, shown, sizeof(s_alert_text));
    if (s_alert_text[0] == '\0') {
        lv_obj_add_flag(s_alert, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(s_alert_label, s_alert_text);
    lv_obj_remove_flag(s_alert, LV_OBJ_FLAG_HIDDEN);
}

static void apply_brightness(const app_model_t *model, const struct tm *local, bool synced)
{
    int percent = model->cfg.brightness_day;
    if (synced) {
        int minute = local->tm_hour * 60 + local->tm_min;
        if (brightness_is_dim(minute, model->cfg.dim_start_min, model->cfg.dim_end_min)) {
            percent = model->cfg.brightness_night;
        }
    }
    if (percent >= 100) {
        lv_obj_add_flag(s_dim, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_opa_t opa = (lv_opa_t)((100 - percent) * 255 / 100);
    lv_obj_set_style_bg_opa(s_dim, opa, 0);
    lv_obj_remove_flag(s_dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_dim);
}

static bool weather_fresh(const app_model_t *model)
{
    if (!model->weather_ok || !model->time_synced || model->weather_at == 0) {
        return false;
    }
    int minutes = clock_refresh_allowed(model->cfg.refresh_minutes) ? model->cfg.refresh_minutes : CLOCK_REFRESH_DEFAULT;
    time_t limit = (time_t)minutes * 4 * 60;
    return (time(NULL) - model->weather_at) < limit;
}

static void gesture_cb(lv_event_t *event)
{
    if (ui_config_is_open()) {
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    bool top_left = point.x < CLOCK_GESTURE_PX && point.y < CLOCK_GESTURE_PX;
    bool bottom_right = point.x >= 800 - CLOCK_GESTURE_PX && point.y >= 480 - CLOCK_GESTURE_PX;
    if (!top_left && !bottom_right) {
        s_gesture_count = 0;
        return;
    }
    int corner = top_left ? 1 : 2;
    uint32_t now = lv_tick_get();
    if (s_gesture_count == 0 || corner != s_gesture_corner || (now - s_gesture_tick) > CLOCK_GESTURE_MS) {
        s_gesture_count = 1;
        s_gesture_corner = corner;
        s_gesture_tick = now;
        return;
    }
    s_gesture_count++;
    s_gesture_tick = now;
    if (s_gesture_count < 3) {
        return;
    }
    s_gesture_count = 0;
    if (corner == 1) {
        ui_config_open();
    } else {
        storage_next_template();
    }
    (void)event;
}

static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    app_model_t model;
    app_state_copy(&model);
    struct tm local;
    memset(&local, 0, sizeof(local));
    if (model.time_synced) {
        time_t now = time(NULL);
        localtime_r(&now, &local);
        char text[8];
        strftime(text, sizeof(text), "%H:%M", &local);
        lv_label_set_text(s_clock, text);
    } else {
        lv_label_set_text(s_clock, "00:00");
    }

    uint8_t clock_font = model.cfg.font_clock <= 2 ? model.cfg.font_clock : 1;
    uint8_t temp_font = model.cfg.font_temp <= 2 ? model.cfg.font_temp : 1;
    uint8_t label_font = model.cfg.font_label <= 2 ? model.cfg.font_label : 1;
    lv_obj_set_style_text_font(s_clock, s_clock_fonts[clock_font], 0);
    lv_obj_set_style_text_font(s_forecast_label, s_label_fonts[label_font], 0);

    bool fresh = weather_fresh(&model);
    bool show_current = model.time_synced && fresh;
    bool show_forecast = false;
    forecast_slot_t slot;
    float forecast_temp = 0;
    float forecast_feels = 0;
    if (show_current) {
        int actual = clock_round_half_away(model.cur_temp);
        int feels = clock_round_half_away(model.cur_feels);
        set_pair(s_current_spans, s_current_left, s_current_right, actual, feels, s_temp_fonts[temp_font]);
        lv_obj_remove_flag(s_current_spans, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_icon_box, LV_OBJ_FLAG_HIDDEN);
        build_weather_icon(wmo_icon(model.cur_code));
        forecast_select(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                        model.cfg.forecast_hour, &slot);
        float precip = 0;
        int prob = 0;
        if (weather_lookup(slot.year, slot.month, slot.day, slot.hour, &forecast_temp, &forecast_feels, &precip, &prob)) {
            show_forecast = true;
            int rounded_temp = clock_round_half_away(forecast_temp);
            int rounded_feels = clock_round_half_away(forecast_feels);
            set_pair(s_forecast_spans, s_forecast_left, s_forecast_right, rounded_temp, rounded_feels, s_temp_fonts[temp_font]);
            lv_label_set_text(s_forecast_label, slot.kind == FORECAST_TODAY ? "dzisiaj" : "jutro 8:00");
        } else {
            weather_request_fetch();
        }
    } else {
        lv_obj_add_flag(s_current_spans, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_icon_box, LV_OBJ_FLAG_HIDDEN);
        s_shown_icon = -1;
    }

    if (show_forecast) {
        lv_obj_remove_flag(s_forecast_spans, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_forecast_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_forecast_spans, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_forecast_label, LV_OBJ_FLAG_HIDDEN);
    }

    int status = -1;
    if (!model.wifi_up || !model.time_synced) {
        status = 0;
    } else if (!fresh) {
        status = 1;
    }
    build_status(status);
    update_alert(&model);
    apply_brightness(&model, &local, model.time_synced);

    int left_band = show_current ? clock_temp_band(clock_round_half_away(model.cur_feels)) : -1;
    int right_band = show_forecast ? clock_temp_band(clock_round_half_away(forecast_feels)) : -1;
    if (left_band != s_shown_left || right_band != s_shown_right || strcmp(model.cfg.template_name, s_shown_name) != 0) {
        s_shown_left = left_band;
        s_shown_right = right_band;
        snprintf(s_shown_name, sizeof(s_shown_name), "%s", model.cfg.template_name);
        storage_request(left_band, right_band);
    }

    uint32_t generation = storage_generation();
    if (generation != s_shown_generation) {
        s_shown_generation = generation;
        if (s_left_img != NULL && storage_left_dsc() != NULL) {
            lv_image_set_src(s_left_img, storage_left_dsc());
        }
        if (s_right_img != NULL && storage_right_dsc() != NULL) {
            lv_image_set_src(s_right_img, storage_right_dsc());
        }
    }
    if (left_band >= 0) {
        lv_obj_remove_flag(s_left_img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_left_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (right_band >= 0) {
        lv_obj_remove_flag(s_right_img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_right_img, LV_OBJ_FLAG_HIDDEN);
    }

    ui_config_tick(&model);
}

static lv_obj_t *make_spans(lv_obj_t *parent, lv_span_t **left, lv_span_t **right)
{
    lv_obj_t *group = lv_spangroup_create(parent);
    lv_spangroup_set_mode(group, LV_SPAN_MODE_EXPAND);
    lv_obj_set_style_text_align(group, LV_TEXT_ALIGN_CENTER, 0);
    *left = lv_spangroup_add_span(group);
    *right = lv_spangroup_add_span(group);
    lv_span_set_text(*left, "");
    lv_span_set_text(*right, "");
    return group;
}

void ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture_cb, LV_EVENT_PRESSED, NULL);

    s_left_img = lv_image_create(screen);
    lv_obj_set_pos(s_left_img, 0, 0);
    lv_obj_add_flag(s_left_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_left_img, LV_OBJ_FLAG_CLICKABLE);
    s_right_img = lv_image_create(screen);
    lv_obj_set_pos(s_right_img, 534, 0);
    lv_obj_add_flag(s_right_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_right_img, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *center = lv_obj_create(screen);
    lv_obj_set_pos(center, 266, 0);
    lv_obj_set_size(center, 268, 480);
    lv_obj_set_style_bg_color(center, lv_color_hex(0x2B2B2B), 0);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(center, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(center, 2, 0);
    lv_obj_set_style_radius(center, 0, 0);
    lv_obj_set_style_pad_all(center, 8, 0);
    lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center, 8, 0);
    lv_obj_remove_flag(center, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_status_box = lv_obj_create(center);
    lv_obj_set_size(s_status_box, 48, 48);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_status_box, 0, 0);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_clock = lv_label_create(center);
    lv_label_set_text(s_clock, "00:00");
    lv_obj_set_style_text_color(s_clock, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_clock, &font_clock_75, 0);

    s_current_spans = make_spans(center, &s_current_left, &s_current_right);
    lv_obj_add_flag(s_current_spans, LV_OBJ_FLAG_HIDDEN);

    s_icon_box = lv_obj_create(center);
    lv_obj_set_size(s_icon_box, 72, 72);
    lv_obj_set_style_bg_opa(s_icon_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_icon_box, 0, 0);
    lv_obj_remove_flag(s_icon_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_icon_box, LV_OBJ_FLAG_HIDDEN);

    s_forecast_spans = make_spans(center, &s_forecast_left, &s_forecast_right);
    lv_obj_add_flag(s_forecast_spans, LV_OBJ_FLAG_HIDDEN);
    s_forecast_label = lv_label_create(center);
    lv_label_set_text(s_forecast_label, "");
    lv_obj_set_style_text_color(s_forecast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_forecast_label, &font_label_28, 0);
    lv_obj_add_flag(s_forecast_label, LV_OBJ_FLAG_HIDDEN);

    s_alert = lv_obj_create(center);
    lv_obj_set_width(s_alert, 248);
    lv_obj_set_height(s_alert, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_alert, lv_color_hex(0x3A2418), 0);
    lv_obj_set_style_bg_opa(s_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_alert, lv_color_hex(0xE0A23A), 0);
    lv_obj_set_style_border_width(s_alert, 2, 0);
    lv_obj_set_style_radius(s_alert, 14, 0);
    lv_obj_set_style_pad_all(s_alert, 12, 0);
    lv_obj_remove_flag(s_alert, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_alert, LV_OBJ_FLAG_HIDDEN);
    s_alert_label = lv_label_create(s_alert);
    lv_obj_set_width(s_alert_label, 220);
    lv_label_set_long_mode(s_alert_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_alert_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_alert_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_alert_label, &font_label_20, 0);
    lv_label_set_text(s_alert_label, "");

    s_dim = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_dim, 800, 480);
    lv_obj_set_style_bg_color(s_dim, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_dim, 0, 0);
    lv_obj_set_style_radius(s_dim, 0, 0);
    lv_obj_remove_flag(s_dim, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_dim, LV_OBJ_FLAG_HIDDEN);

    if (storage_left_dsc() != NULL) {
        lv_image_set_src(s_left_img, storage_left_dsc());
    }
    if (storage_right_dsc() != NULL) {
        lv_image_set_src(s_right_img, storage_right_dsc());
    }
    lv_timer_create(tick_cb, 500, NULL);
}
