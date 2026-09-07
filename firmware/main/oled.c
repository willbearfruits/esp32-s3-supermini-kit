#include "oled.h"
#include <math.h>
#include <stdlib.h>

#include <string.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_log.h"

#include "font5x7.h"

static const char *TAG = "oled";

// Page-major layout, exactly what the SSD1306 wants: fb[page][column],
// bit 0 of each byte is the top pixel of that page.
static uint8_t fb[OLED_H / 8][OLED_W];
static esp_lcd_panel_handle_t panel;

esp_err_t oled_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = addr,
        .scl_speed_hz = 400 * 1000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
    esp_err_t err = esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel io: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_ssd1306_config_t ssd_cfg = { .height = OLED_H };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .vendor_config = &ssd_cfg,
    };
    err = esp_lcd_new_panel_ssd1306(io, &panel_cfg, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new panel: %s", esp_err_to_name(err));
        panel = NULL;
        return err;
    }
    err = esp_lcd_panel_reset(panel);
    if (err == ESP_OK) err = esp_lcd_panel_init(panel);
    if (err == ESP_OK) err = esp_lcd_panel_disp_on_off(panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel init: %s", esp_err_to_name(err));
        panel = NULL;
        return err;
    }
    oled_clear();
    oled_flush();
    return ESP_OK;
}

bool oled_present(void) { return panel != NULL; }

void oled_clear(void) { memset(fb, 0, sizeof(fb)); }

void oled_pixel(int x, int y, bool on)
{
    if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) return;
    if (on) fb[y >> 3][x] |= 1 << (y & 7);
    else    fb[y >> 3][x] &= ~(1 << (y & 7));
}

void oled_hline(int x0, int x1, int y, bool on)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; x++) oled_pixel(x, y, on);
}

void oled_vline(int x, int y0, int y1, bool on)
{
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) oled_pixel(x, y, on);
}

void oled_rect(int x, int y, int w, int h, bool fill)
{
    if (w <= 0 || h <= 0) return;
    if (fill) {
        for (int yy = y; yy < y + h; yy++) oled_hline(x, x + w - 1, yy, true);
    } else {
        oled_hline(x, x + w - 1, y, true);
        oled_hline(x, x + w - 1, y + h - 1, true);
        oled_vline(x, y, y + h - 1, true);
        oled_vline(x + w - 1, y, y + h - 1, true);
    }
}

void oled_text_c(int x, int y, const char *s, bool on)
{
    for (; *s; s++, x += 6) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c > 0x7F) c = '?';
        const uint8_t *g = font5x7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) oled_pixel(x + col, y + row, on);
            }
        }
    }
}
void oled_text(int x, int y, const char *s) { oled_text_c(x, y, s, true); }

void oled_text2(int x, int y, const char *s)
{
    for (; *s; s++, x += 12) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c > 0x7F) c = '?';
        const uint8_t *g = font5x7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) oled_rect(x + 2 * col, y + 2 * row, 2, 2, true);
            }
        }
    }
}

void oled_circle(int cx, int cy, int r, bool fill)
{
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
        if (fill) oled_hline(cx - dx, cx + dx, cy + dy, true);
        else { oled_pixel(cx - dx, cy + dy, true); oled_pixel(cx + dx, cy + dy, true); }
    }
    if (!fill) { oled_pixel(cx, cy - r, true); oled_pixel(cx, cy + r, true); }
}

void oled_line(int x0, int y0, int x1, int y1, bool on)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        oled_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void oled_bitmap(int x, int y, int w, int h, const uint8_t *rows, bool on)
{
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            if (rows[r] & (0x80 >> c)) oled_pixel(x + c, y + r, on);
}

void oled_flush(void)
{
    if (!panel) return;
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, OLED_W, OLED_H, fb);
    if (err != ESP_OK) ESP_LOGW(TAG, "draw: %s", esp_err_to_name(err));
}
