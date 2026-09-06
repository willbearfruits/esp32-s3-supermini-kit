// Minimal SSD1306 128x64 driver on top of esp_lcd, with a 1 KB framebuffer.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#define OLED_W 128
#define OLED_H 64

esp_err_t oled_init(i2c_master_bus_handle_t bus, uint8_t addr);
bool      oled_present(void);

void oled_clear(void);
void oled_pixel(int x, int y, bool on);
void oled_hline(int x0, int x1, int y, bool on);
void oled_vline(int x, int y0, int y1, bool on);
void oled_rect(int x, int y, int w, int h, bool fill);
void oled_text(int x, int y, const char *s);   // 5x7 glyphs, 6 px advance
void oled_flush(void);                          // push framebuffer to panel
