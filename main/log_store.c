#include "log_store.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_state.h"
#include "sdcard.h"
#include "time_sync.h"

static const char *TAG = "log";

typedef struct {
    log_level_t level;
    char text[180];
} log_line_t;

static QueueHandle_t s_queue;
static bool s_storage;
static log_line_t s_backlog[32];
static int s_backlog_count;
static char s_open_path[64];
static FILE *s_file;

static const char *level_name(log_level_t level)
{
    if (level == LOG_ERROR) {
        return "BLAD";
    }
    if (level == LOG_WARN) {
        return "UWAGA";
    }
    return "INFO";
}

bool log_safe_name(const char *name)
{
    if (name == NULL) {
        return false;
    }
    if (strcmp(name, "niesynchronizowane.log") == 0) {
        return true;
    }
    if (strlen(name) != 14 || strcmp(name + 10, ".log") != 0) {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (name[i] != '-') {
                return false;
            }
        } else if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    return true;
}

static bool copy_log_name(char *dst, size_t cap, const char *src)
{
    if (!log_safe_name(src)) {
        return false;
    }
    size_t length = strlen(src);
    if (length == 0 || length >= cap) {
        return false;
    }
    memcpy(dst, src, length + 1);
    return true;
}

static void close_file(void)
{
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    s_open_path[0] = '\0';
}

static void choose_path(char *out, size_t cap)
{
    if (!time_is_synced()) {
        char newest[32] = "";
        time_t newest_time = 0;
        DIR *dir = opendir("/sdcard/logs");
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                char name[32];
                if (!copy_log_name(name, sizeof(name), entry->d_name)) {
                    continue;
                }
                char full[80];
                snprintf(full, sizeof(full), "/sdcard/logs/%s", name);
                struct stat st;
                if (stat(full, &st) == 0 && st.st_mtime >= newest_time) {
                    newest_time = st.st_mtime;
                    memcpy(newest, name, sizeof(newest));
                }
            }
            closedir(dir);
        }
        if (newest[0] == '\0') {
            snprintf(out, cap, "/sdcard/logs/niesynchronizowane.log");
        } else {
            snprintf(out, cap, "/sdcard/logs/%s", newest);
        }
        return;
    }

    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char name[32];
    strftime(name, sizeof(name), "%Y-%m-%d.log", &local);
    snprintf(out, cap, "/sdcard/logs/%s", name);
}

static void rotate_if_needed(const char *path)
{
    char name[32];
    const char *slash = strrchr(path, '/');
    snprintf(name, sizeof(name), "%s", slash != NULL ? slash + 1 : path);

    typedef struct {
        char name[32];
        time_t mtime;
    } item_t;
    item_t items[40];
    int count = 0;
    bool exists = false;
    DIR *dir = opendir("/sdcard/logs");
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < 40) {
        char item_name[32];
        if (!copy_log_name(item_name, sizeof(item_name), entry->d_name)) {
            continue;
        }
        if (strcmp(item_name, name) == 0) {
            exists = true;
        }
        char full[80];
        snprintf(full, sizeof(full), "/sdcard/logs/%s", item_name);
        struct stat st;
        if (stat(full, &st) != 0) {
            continue;
        }
        memcpy(items[count].name, item_name, sizeof(items[count].name));
        items[count].mtime = st.st_mtime;
        count++;
    }
    closedir(dir);
    if (exists || count < 30) {
        return;
    }
    while (count >= 30) {
        int oldest = 0;
        for (int i = 1; i < count; i++) {
            if (items[i].mtime < items[oldest].mtime) {
                oldest = i;
            }
        }
        char full[80];
        snprintf(full, sizeof(full), "/sdcard/logs/%s", items[oldest].name);
        remove(full);
        items[oldest] = items[count - 1];
        count--;
    }
}

static void write_line(const log_line_t *line)
{
    if (!s_storage || !sdcard_mounted()) {
        return;
    }
    sdcard_lock();
    char path[64];
    choose_path(path, sizeof(path));
    if (strcmp(path, s_open_path) != 0) {
        close_file();
        rotate_if_needed(path);
        s_file = fopen(path, "a");
        if (s_file != NULL) {
            snprintf(s_open_path, sizeof(s_open_path), "%s", path);
        }
    }
    if (s_file == NULL) {
        sdcard_unlock();
        return;
    }

    char prefix[48];
    if (!time_is_synced()) {
        int64_t ms = esp_timer_get_time() / 1000;
        snprintf(prefix, sizeof(prefix), "unsynced +%lld ms", (long long)ms);
    } else {
        time_t now = time(NULL);
        struct tm local;
        localtime_r(&now, &local);
        strftime(prefix, sizeof(prefix), "%Y-%m-%d %H:%M:%S", &local);
    }
    fprintf(s_file, "%s %s %s\n", prefix, level_name(line->level), line->text);
    fflush(s_file);
    fsync(fileno(s_file));
    sdcard_unlock();
}

static void log_task(void *arg)
{
    (void)arg;
    log_line_t line;
    while (true) {
        if (xQueueReceive(s_queue, &line, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (!s_storage) {
                if (s_backlog_count < 32) {
                    s_backlog[s_backlog_count++] = line;
                }
            } else {
                for (int i = 0; i < s_backlog_count; i++) {
                    write_line(&s_backlog[i]);
                }
                s_backlog_count = 0;
                write_line(&line);
            }
        }
    }
}

void log_init(void)
{
    s_queue = xQueueCreate(32, sizeof(log_line_t));
    xTaskCreate(log_task, "log", 4096, NULL, 2, NULL);
}

void log_bind_storage(bool enabled)
{
    s_storage = enabled;
    if (!enabled) {
        sdcard_lock();
        close_file();
        sdcard_unlock();
    }
}

void log_event(log_level_t level, const char *fmt, ...)
{
    log_line_t line;
    line.level = level;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line.text, sizeof(line.text), fmt, ap);
    va_end(ap);
    if (level == LOG_ERROR) {
        ESP_LOGE(TAG, "%s", line.text);
    } else if (level == LOG_WARN) {
        ESP_LOGW(TAG, "%s", line.text);
    } else {
        ESP_LOGI(TAG, "%s", line.text);
    }
    if (s_queue != NULL) {
        xQueueSend(s_queue, &line, 0);
    }
}

int log_list(char names[][32], int capacity)
{
    typedef struct {
        char name[32];
        time_t mtime;
    } item_t;
    item_t items[40];
    int count = 0;
    if (!s_storage) {
        return 0;
    }
    sdcard_lock();
    DIR *dir = opendir("/sdcard/logs");
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && count < 40) {
            char item_name[32];
            if (!copy_log_name(item_name, sizeof(item_name), entry->d_name)) {
                continue;
            }
            char full[80];
            snprintf(full, sizeof(full), "/sdcard/logs/%s", item_name);
            struct stat st;
            if (stat(full, &st) != 0) {
                continue;
            }
            memcpy(items[count].name, item_name, sizeof(items[count].name));
            items[count].mtime = st.st_mtime;
            count++;
        }
        closedir(dir);
    }
    sdcard_unlock();

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (items[j].mtime > items[i].mtime) {
                item_t tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }
    int out = count < capacity ? count : capacity;
    for (int i = 0; i < out; i++) {
        snprintf(names[i], 32, "%s", items[i].name);
    }
    return out;
}
