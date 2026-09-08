// Hardware test: the sine sweep from fx_sine.c plus an OLED exercise.
// The screen cycles through a border/grid page (every edge and corner
// lit), a text page, and an animation, three seconds each.
#include "app.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"

static const char *TAG = "test";
float fx_sine_db(void);

static void page_grid(int frame)
{
    oled_rect(0, 0, OLED_W, OLED_H, false);                 // outer border: all four edges
    for (int x = 0; x < OLED_W; x += 8) oled_vline(x, 0, OLED_H - 1, true);
    for (int y = 0; y < OLED_H; y += 8) oled_hline(0, OLED_W - 1, y, true);
    // corner squares, blinking, so a dead corner is obvious
    bool on = (frame / 12) & 1;
    oled_rect(0, 0, 6, 6, on); oled_rect(OLED_W - 6, 0, 6, 6, on);
    oled_rect(0, OLED_H - 6, 6, 6, on); oled_rect(OLED_W - 6, OLED_H - 6, 6, 6, on);
    oled_text(40, 28, "128 x 64 grid");
}

static void page_text(float db)
{
    char line[32];
    oled_text2(4, 2, "OLED OK");
    oled_text(4, 20, "ABCDEFGHIJKLMNOPQRSTU");
    oled_text(4, 29, "abcdefghijklmnopqrstu");
    oled_text(4, 38, "0123456789 !\"#$%&'()*+");
    snprintf(line, sizeof line, "sine 440 Hz %5.1f dB", db);
    oled_text(4, 50, line);
}

static void page_anim(int frame, float db)
{
    // bouncing ball on a diagonal, a rotating spoke, and the level bar
    int x = frame % 220, y = frame % 92;
    if (x > 110) x = 220 - x;
    if (y > 46) y = 92 - y;
    oled_circle(9 + x, 9 + y, 6, true);
    float a = frame * 0.12f;
    oled_line(100, 24, 100 + (int)(16 * cosf(a)), 24 + (int)(16 * sinf(a)), true);
    oled_circle(100, 24, 17, false);
    int bar = (int)((db + 50) / 50 * 118);
    oled_rect(4, 54, 120, 8, false);
    oled_rect(5, 55, bar > 0 ? bar : 0, 6, true);
}

static void i2c_rescan(i2c_master_bus_handle_t bus)
{
    char found[64] = "";
    int n = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            char t[8]; snprintf(t, sizeof t, " 0x%02X", a); strlcat(found, t, sizeof found); n++;
            if (!oled_present() && (a == 0x3C || a == 0x3D)) {
                if (oled_init(bus, a) == ESP_OK) ESP_LOGI(TAG, "OLED came up at 0x%02X", a);
            }
        }
    }
    ESP_LOGI(TAG, "I2C scan: %s%s", n ? found : " nothing answers", n ? "" : " (check SDA=GPIO12 SCL=GPIO13, VCC 3V3, GND)");
}

void app_test_run(i2c_master_bus_handle_t bus)
{
    ESP_LOGI(TAG, "sine 440 Hz, level sweeps -50..0 dB every 8 s, %d Hz, both DAC outputs; OLED %s",
             CONFIG_KIT_SAMPLE_RATE, oled_present() ? "found, cycling grid / text / animation" : "not found");
    int frame = 0;
    int64_t last_log = 0, last_scan = 0;
    while (1) {
        int64_t now = esp_timer_get_time();
        float db = fx_sine_db();
        if (now - last_scan > 2000000 && !oled_present()) { last_scan = now; i2c_rescan(bus); }
        if (now - last_log > 500000) {
            last_log = now;
            int bar = (int)((db + 50) / 50 * 30);
            char line[40];
            snprintf(line, sizeof line, "|%-30.*s|", bar, "##############################");
            ESP_LOGI(TAG, "sine 440 Hz  %5.1f dB %s  peak %5.2f  cpu %2.0f%%  oled %s", db, line, audio_out_peak(),
                     audio_cpu_load() * 100, oled_present() ? "on" : "none");
        }
        if (oled_present()) {
            oled_clear();
            int phase = (frame / 75) % 3;                     // 3 s per page at 25 fps
            if (phase == 0) page_grid(frame);
            else if (phase == 1) page_text(db);
            else page_anim(frame, db);
            oled_flush();
        }
        frame++;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
