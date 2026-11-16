#include "image_decode.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "png.h"

static uint16_t pack565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void fit_rgba(const uint8_t *rgba, int width, int height, uint16_t *dest)
{
    const uint16_t background = pack565(0x40, 0x40, 0x40);
    for (int i = 0; i < CLOCK_IMAGE_W * CLOCK_IMAGE_H; i++) {
        dest[i] = background;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    int dest_w = CLOCK_IMAGE_W;
    int dest_h = (int)((long)height * CLOCK_IMAGE_W / width);
    if (dest_h > CLOCK_IMAGE_H) {
        dest_h = CLOCK_IMAGE_H;
        dest_w = (int)((long)width * CLOCK_IMAGE_H / height);
    }
    if (dest_w < 1) {
        dest_w = 1;
    }
    if (dest_h < 1) {
        dest_h = 1;
    }
    int origin_x = (CLOCK_IMAGE_W - dest_w) / 2;
    int origin_y = (CLOCK_IMAGE_H - dest_h) / 2;
    for (int y = 0; y < dest_h; y++) {
        int source_y = y * height / dest_h;
        if (source_y >= height) {
            source_y = height - 1;
        }
        for (int x = 0; x < dest_w; x++) {
            int source_x = x * width / dest_w;
            if (source_x >= width) {
                source_x = width - 1;
            }
            const uint8_t *pixel = rgba + ((size_t)source_y * (size_t)width + (size_t)source_x) * 4;
            uint8_t alpha = pixel[3];
            uint8_t r = (uint8_t)((pixel[0] * alpha + 0x40 * (255 - alpha)) / 255);
            uint8_t g = (uint8_t)((pixel[1] * alpha + 0x40 * (255 - alpha)) / 255);
            uint8_t b = (uint8_t)((pixel[2] * alpha + 0x40 * (255 - alpha)) / 255);
            dest[(origin_y + y) * CLOCK_IMAGE_W + origin_x + x] = pack565(r, g, b);
        }
    }
}

bool image_decode_png(const uint8_t *data, size_t length, uint16_t *dest)
{
    if (data == NULL || dest == NULL || length == 0 || length > CLOCK_IMAGE_MAX_BYTES) {
        return false;
    }
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, data, length)) {
        png_image_free(&image);
        return false;
    }
    if (image.width == 0 || image.height == 0 || image.width > CLOCK_IMAGE_MAX_SIDE ||
        image.height > CLOCK_IMAGE_MAX_SIDE) {
        png_image_free(&image);
        return false;
    }
    image.format = PNG_FORMAT_RGBA;
    size_t bytes = PNG_IMAGE_SIZE(image);
    uint8_t *rgba = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (rgba == NULL) {
        rgba = malloc(bytes);
    }
    if (rgba == NULL) {
        png_image_free(&image);
        return false;
    }
    if (!png_image_finish_read(&image, NULL, rgba, 0, NULL)) {
        free(rgba);
        png_image_free(&image);
        return false;
    }
    fit_rgba(rgba, (int)image.width, (int)image.height, dest);
    free(rgba);
    png_image_free(&image);
    return true;
}

void image_fill_fallback(uint16_t *dest, int band)
{
    static const uint16_t colors[] = {
        0x0378, 0x24D3, 0x34A6, 0xEBA3, 0xF660,
    };
    uint16_t color = colors[band < 0 || band > 4 ? 2 : band];
    for (int i = 0; i < CLOCK_IMAGE_W * CLOCK_IMAGE_H; i++) {
        dest[i] = color;
    }
}
