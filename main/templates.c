#include "templates.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "app_state.h"
#include "clock_logic.h"
#include "config_store.h"
#include "image_decode.h"
#include "log_store.h"
#include "sdcard.h"

extern const uint8_t fallback_1_start[] asm("_binary_1_png_start");
extern const uint8_t fallback_1_end[] asm("_binary_1_png_end");
extern const uint8_t fallback_2_start[] asm("_binary_2_png_start");
extern const uint8_t fallback_2_end[] asm("_binary_2_png_end");
extern const uint8_t fallback_3_start[] asm("_binary_3_png_start");
extern const uint8_t fallback_3_end[] asm("_binary_3_png_end");
extern const uint8_t fallback_4_start[] asm("_binary_4_png_start");
extern const uint8_t fallback_4_end[] asm("_binary_4_png_end");
extern const uint8_t fallback_5_start[] asm("_binary_5_png_start");
extern const uint8_t fallback_5_end[] asm("_binary_5_png_end");

typedef struct {
    char name[CLOCK_TEMPLATE_NAME_LEN];
    char file[5][16];
} template_entry_t;

static template_entry_t s_entries[CLOCK_TEMPLATE_MAX];
static int s_count;
static uint16_t *s_left_px;
static uint16_t *s_right_px;
static lv_image_dsc_t s_left_dsc;
static lv_image_dsc_t s_right_dsc;
static SemaphoreHandle_t s_lock;
static int s_want_left = -1;
static int s_want_right = -1;
static int s_have_left = -2;
static int s_have_right = -2;
static char s_have_name[CLOCK_TEMPLATE_NAME_LEN];
static uint32_t s_generation;

static const uint8_t *fallback_start(int band)
{
    switch (band) {
    case 0:
        return fallback_1_start;
    case 1:
        return fallback_2_start;
    case 2:
        return fallback_3_start;
    case 3:
        return fallback_4_start;
    default:
        return fallback_5_start;
    }
}

static const uint8_t *fallback_end(int band)
{
    switch (band) {
    case 0:
        return fallback_1_end;
    case 1:
        return fallback_2_end;
    case 2:
        return fallback_3_end;
    case 3:
        return fallback_4_end;
    default:
        return fallback_5_end;
    }
}

static void make_dsc(lv_image_dsc_t *dsc, const uint16_t *pixels)
{
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.w = CLOCK_IMAGE_W;
    dsc->header.h = CLOCK_IMAGE_H;
    dsc->header.stride = CLOCK_IMAGE_W * 2;
    dsc->data_size = CLOCK_IMAGE_W * CLOCK_IMAGE_H * 2;
    dsc->data = (const uint8_t *)pixels;
}

static uint16_t *alloc_frame(void)
{
    size_t bytes = CLOCK_IMAGE_W * CLOCK_IMAGE_H * sizeof(uint16_t);
    uint16_t *pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pixels == NULL) {
        pixels = malloc(bytes);
    }
    return pixels;
}

static void paint_fallback(uint16_t *dest, int band)
{
    const uint8_t *start = fallback_start(band);
    const uint8_t *end = fallback_end(band);
    if (!image_decode_png(start, (size_t)(end - start), dest)) {
        image_fill_fallback(dest, band);
    }
}

static bool names_match(const char *have, const char *need)
{
    return clock_name_cmp(have, need) == 0;
}

static bool directory_complete(const char *dir_name, template_entry_t *entry)
{
    char path[160];
    snprintf(path, sizeof(path), "/sdcard/images/%s", dir_name);
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return false;
    }
    char found[5][16];
    memset(found, 0, sizeof(found));
    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
        for (int band = 0; band < 5; band++) {
            char need[8];
            snprintf(need, sizeof(need), "%d.png", band + 1);
            size_t name_len = strlen(item->d_name);
            if (names_match(item->d_name, need) && name_len < sizeof(found[band])) {
                memcpy(found[band], item->d_name, name_len + 1);
            }
        }
    }
    closedir(dir);

    char missing[64] = "";
    bool ok = true;
    for (int band = 0; band < 5; band++) {
        if (found[band][0] == '\0') {
            ok = false;
            char piece[8];
            snprintf(piece, sizeof(piece), "%s%d.png", missing[0] ? " " : "", band + 1);
            strncat(missing, piece, sizeof(missing) - strlen(missing) - 1);
        }
    }
    if (!ok) {
        log_event(LOG_WARN, "Szablon %s pominięty, brak: %s", dir_name, missing);
        return false;
    }
    snprintf(entry->name, sizeof(entry->name), "%s", dir_name);
    for (int band = 0; band < 5; band++) {
        snprintf(entry->file[band], sizeof(entry->file[band]), "%s", found[band]);
    }
    return true;
}

static int cmp_entry(const void *a, const void *b)
{
    const template_entry_t *left = a;
    const template_entry_t *right = b;
    return clock_name_cmp(left->name, right->name);
}

static void scan_templates(void)
{
    s_count = 0;
    DIR *dir = opendir("/sdcard/images");
    if (dir == NULL) {
        app_state_lock();
        app_model_t *model = app_state_edit();
        model->template_count = 0;
        snprintf(model->template_note, sizeof(model->template_note), "%s", "Brak kompletnych szablonów");
        app_state_bump();
        app_state_unlock();
        log_event(LOG_WARN, "Brak katalogu obrazów na karcie");
        return;
    }
    struct dirent *item;
    while ((item = readdir(dir)) != NULL && s_count < CLOCK_TEMPLATE_MAX) {
        if (item->d_name[0] == '.') {
            continue;
        }
        size_t name_len = strlen(item->d_name);
        if (name_len >= CLOCK_TEMPLATE_NAME_LEN || !clock_template_name_ok(item->d_name)) {
            log_event(LOG_WARN, "Pominięto katalog szablonu o niedozwolonej nazwie");
            continue;
        }
        char dir_name[CLOCK_TEMPLATE_NAME_LEN];
        memcpy(dir_name, item->d_name, name_len + 1);
        char path[160];
        snprintf(path, sizeof(path), "/sdcard/images/%s", dir_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        template_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        sdcard_lock();
        bool complete = directory_complete(dir_name, &entry);
        sdcard_unlock();
        if (complete) {
            s_entries[s_count++] = entry;
        }
    }
    closedir(dir);
    qsort(s_entries, (size_t)s_count, sizeof(s_entries[0]), cmp_entry);

    app_state_lock();
    app_model_t *model = app_state_edit();
    model->template_count = s_count;
    for (int i = 0; i < s_count; i++) {
        snprintf(model->templates[i], CLOCK_TEMPLATE_NAME_LEN, "%s", s_entries[i].name);
    }
    if (s_count == 0) {
        model->cfg.template_name[0] = '\0';
        snprintf(model->template_note, sizeof(model->template_note), "%s", "Brak kompletnych szablonów");
        log_event(LOG_WARN, "Karta nie ma kompletnego szablonu");
    } else {
        int found = -1;
        for (int i = 0; i < s_count; i++) {
            if (names_match(s_entries[i].name, model->cfg.template_name)) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            snprintf(model->cfg.template_name, sizeof(model->cfg.template_name), "%s", s_entries[0].name);
            snprintf(model->template_note, sizeof(model->template_note), "%s", "Zapisany szablon zastąpiono pierwszym");
            log_event(LOG_WARN, "Zapisany szablon niedostępny, wybrano %s", s_entries[0].name);
        } else {
            model->template_note[0] = '\0';
        }
    }
    clock_config_t copy = model->cfg;
    bool save = s_count == 0 || model->template_note[0] != '\0';
    app_state_bump();
    app_state_unlock();
    if (save) {
        config_store_save(&copy);
    }
}

static bool read_band_file(const template_entry_t *entry, int band, uint16_t *dest)
{
    char path[192];
    snprintf(path, sizeof(path), "/sdcard/images/%s/%s", entry->name, entry->file[band]);
    sdcard_lock();
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        sdcard_unlock();
        log_event(LOG_WARN, "Nie można otworzyć %s/%s", entry->name, entry->file[band]);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        sdcard_unlock();
        return false;
    }
    long length = ftell(file);
    if (length <= 0 || length > CLOCK_IMAGE_MAX_BYTES) {
        fclose(file);
        sdcard_unlock();
        log_event(LOG_WARN, "Plik %s/%s ma niedozwolony rozmiar", entry->name, entry->file[band]);
        return false;
    }
    uint8_t *data = heap_caps_malloc((size_t)length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data == NULL) {
        data = malloc((size_t)length);
    }
    if (data == NULL) {
        fclose(file);
        sdcard_unlock();
        return false;
    }
    rewind(file);
    size_t got = fread(data, 1, (size_t)length, file);
    fclose(file);
    sdcard_unlock();
    bool ok = got == (size_t)length && image_decode_png(data, got, dest);
    free(data);
    if (!ok) {
        log_event(LOG_WARN, "Nie można wczytać %s/%s", entry->name, entry->file[band]);
    }
    return ok;
}

static void paint_band(uint16_t *dest, int band, const char *name)
{
    if (band < 0 || band > 4) {
        return;
    }
    const template_entry_t *entry = NULL;
    for (int i = 0; i < s_count; i++) {
        if (names_match(s_entries[i].name, name)) {
            entry = &s_entries[i];
            break;
        }
    }
    if (entry != NULL && read_band_file(entry, band, dest)) {
        return;
    }
    if (entry != NULL) {
        app_state_lock();
        app_model_t *model = app_state_edit();
        snprintf(model->template_note, sizeof(model->template_note), "Uszkodzony plik %d.png w %s", band + 1, entry->name);
        app_state_bump();
        app_state_unlock();
    }
    paint_fallback(dest, band);
}

static void storage_task(void *arg)
{
    (void)arg;
    sd_status_t status = sdcard_mount();
    app_state_lock();
    app_model_t *model = app_state_edit();
    if (status == SD_OK) {
        model->sd_ok = true;
        model->logs_on = true;
        model->sd_error[0] = '\0';
    } else if (status == SD_FULL) {
        model->sd_ok = false;
        model->sd_full = true;
        model->logs_on = false;
        snprintf(model->sd_error, sizeof(model->sd_error), "%s",
                 sdcard_last_error()[0] != '\0' ? sdcard_last_error() : "Karta pamięci jest pełna");
    } else {
        model->sd_ok = false;
        model->logs_on = false;
        snprintf(model->sd_error, sizeof(model->sd_error), "%s",
                 sdcard_last_error()[0] != '\0' ? sdcard_last_error() : "Brak karty pamięci");
    }
    app_state_bump();
    app_state_unlock();

    if (status == SD_OK) {
        mkdir("/sdcard/logs", 0775);
        log_bind_storage(true);
        log_event(LOG_INFO, "Karta TF gotowa");
        scan_templates();
    } else {
        log_bind_storage(false);
        log_event(LOG_ERROR, "Karta TF: %s", status == SD_FULL ? "pełna" : "niedostępna");
    }

    while (true) {
        int left;
        int right;
        char name[CLOCK_TEMPLATE_NAME_LEN];
        xSemaphoreTake(s_lock, portMAX_DELAY);
        left = s_want_left;
        right = s_want_right;
        xSemaphoreGive(s_lock);
        app_model_t model_copy;
        app_state_copy(&model_copy);
        snprintf(name, sizeof(name), "%s", model_copy.cfg.template_name);

        if (left == s_have_left && right == s_have_right && strcmp(name, s_have_name) == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (left >= 0) {
            paint_band(s_left_px, left, name);
        }
        if (right >= 0) {
            paint_band(s_right_px, right, name);
        }
        s_have_left = left;
        s_have_right = right;
        snprintf(s_have_name, sizeof(s_have_name), "%s", name);
        s_generation++;
    }
}

void storage_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_left_px = alloc_frame();
    s_right_px = alloc_frame();
    if (s_left_px != NULL) {
        paint_fallback(s_left_px, 2);
        make_dsc(&s_left_dsc, s_left_px);
    }
    if (s_right_px != NULL) {
        paint_fallback(s_right_px, 2);
        make_dsc(&s_right_dsc, s_right_px);
    }
    xTaskCreate(storage_task, "storage", 16384, NULL, 3, NULL);
}

void storage_request(int left_band, int right_band)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_want_left = left_band;
    s_want_right = right_band;
    xSemaphoreGive(s_lock);
}

void storage_next_template(void)
{
    app_state_lock();
    app_model_t *model = app_state_edit();
    if (model->template_count < 2) {
        app_state_unlock();
        return;
    }
    int index = 0;
    for (int i = 0; i < model->template_count; i++) {
        if (names_match(model->templates[i], model->cfg.template_name)) {
            index = i;
            break;
        }
    }
    index = (index + 1) % model->template_count;
    snprintf(model->cfg.template_name, sizeof(model->cfg.template_name), "%s", model->templates[index]);
    clock_config_t copy = model->cfg;
    app_state_unlock();
    config_store_save(&copy);
    log_event(LOG_INFO, "Szablon zmieniony gestem: %s", copy.template_name);
}

uint32_t storage_generation(void)
{
    return s_generation;
}

const lv_image_dsc_t *storage_left_dsc(void)
{
    return s_left_px != NULL ? &s_left_dsc : NULL;
}

const lv_image_dsc_t *storage_right_dsc(void)
{
    return s_right_px != NULL ? &s_right_dsc : NULL;
}
