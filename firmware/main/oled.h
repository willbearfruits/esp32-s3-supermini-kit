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
void oled_text_c(int x, int y, const char *s, bool on);   // same, choose colour
void oled_text2(int x, int y, const char *s);  // 2x glyphs, 12 px advance, 14 px tall
void oled_circle(int cx, int cy, int r, bool fill);
void oled_line(int x0, int y0, int x1, int y1, bool on);
void oled_bitmap(int x, int y, int w, int h, const uint8_t *rows, bool on); // w<=8, one byte per row, MSB left
void oled_flush(void);                          // push framebuffer to panel
