// Kit voice instrument.
//
// Sing or talk into the mic and the active mode turns it into sound on the
// DACs. The BOOT button steps through the modes; the OLED shows the mode,
// what the voice tracker hears, an output meter and the output waveform.
// Over USB the board is a MIDI device (voice-to-MIDI out, MIDI in plays the
// guitar modes) plus a serial console.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "pins.h"
#include "oled.h"
#include "audio.h"
#include "fx.h"
#include "dsp.h"
#include "selftest.h"
#include "usb_midi.h"

static const char *TAG = "kit";

#define OLED_ADDR 0x3C
#define PIN_BOOT  0

static void gpio_setup(void)
{
    gpio_config_t amp = { .pin_bit_mask = 1ULL << PIN_AMP_SD, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&amp));
    gpio_set_level(PIN_AMP_SD, 0);

    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_ENC_SW) | (1ULL << PIN_BOOT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));
}

static i2c_master_bus_handle_t i2c_setup(void)
{
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) ESP_LOGI(TAG, "I2C device at 0x%02X", a);
    }
    return bus;
}

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

void app_main(void)
{
    usb_midi_init();                 // console moves to USB CDC from here on
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "kit voice instrument, %d modes, %d Hz", fx_count, CONFIG_KIT_SAMPLE_RATE);

    selftest_pins();
    gpio_setup();

    i2c_master_bus_handle_t bus = i2c_setup();
    if (oled_init(bus, OLED_ADDR) == ESP_OK) ESP_LOGI(TAG, "OLED up");
    else ESP_LOGW(TAG, "no OLED, continuing without display");

    audio_start();

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
