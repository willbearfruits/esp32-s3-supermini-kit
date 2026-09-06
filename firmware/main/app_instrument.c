// Voice instrument UI: BOOT steps through the modes, the OLED shows the mode,
// what the voice tracker hears, an output meter and the output waveform.
#include "app.h"
#include "audio.h"
#include "fx.h"
#include "dsp.h"
#include "oled.h"
#include "pins.h"
#include "usb_midi.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "kit";
#define PIN_BOOT 0

static void draw(void)
{
    char line[32], nm[5];
    int m = audio_get_mode();
    const fx_t *fx = fx_list[m];
    voice_t v;
    audio_get_voice(&v);

    oled_clear();
    snprintf(line, sizeof line, "%d/%d %s", m + 1, fx_count, fx->name);
    oled_text(0, 0, line);
    oled_hline(0, OLED_W - 1, 8, true);

    fx->status(line, sizeof line);
    oled_text(0, 11, line);

    if (v.voiced) snprintf(line, sizeof line, "%-3s %4.0fHz %3.0fdB", note_name(v.note, nm), v.freq, v.db);
    else snprintf(line, sizeof line, "%s %3.0fdB", v.gate ? "..." : "---", v.db);
    oled_text(0, 21, line);
    oled_text(OLED_W - 5 * 6, 21, usb_midi_connected() ? "MIDI" : "    ");

    float pk = audio_out_peak();
    float db = pk > 1e-5f ? 20 * log10f(pk) : -60;
    int w = (int)((db + 60) / 60 * (OLED_W - 2));
    if (w < 0) w = 0;
    if (w > OLED_W - 2) w = OLED_W - 2;
    oled_rect(0, 30, OLED_W, 5, false);
    oled_rect(1, 31, w, 3, true);

    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    const int cy = 49;
    for (int x = 0; x < OLED_W; x += 4) oled_pixel(x, cy, true);
    int prev = cy - (wave[0] * 14) / 128;
    for (int x = 1; x < OLED_W; x++) {
        int y = cy - (wave[x] * 14) / 128;
        oled_vline(x, prev, y, true);
        prev = y;
    }
    oled_flush();
}

void app_instrument_run(void)
{
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_BOOT, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&in));

    int last_btn = 1, stable = 1, debounce = 0;
    int64_t last_draw = 0, last_log = 0;
    while (1) {
        // BOOT button, active low, 30 ms debounce, acts on release
        int b = gpio_get_level(PIN_BOOT);
        if (b != last_btn) { debounce = 0; last_btn = b; }
        else if (++debounce == 3 && b != stable) {
            stable = b;
            if (stable == 1) {
                int next = (audio_get_mode() + 1) % fx_count;
                audio_set_mode(next);
                usb_midi_program_change(next);
            }
        }
        int64_t now = esp_timer_get_time();
        if (oled_present() && now - last_draw > 50000) { draw(); last_draw = now; }
        if (now - last_log > 1000000) {
            last_log = now;
            voice_t v; char nm[5];
            audio_get_voice(&v);
            ESP_LOGI(TAG, "%-13s %s %5.1f dB  %s %6.1f Hz conf %.2f  midi %s",
                     fx_list[audio_get_mode()]->name, v.gate ? "VOICE" : "quiet", v.db,
                     v.voiced ? note_name(v.note, nm) : "---", v.freq, v.conf,
                     usb_midi_connected() ? "on" : "off");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
