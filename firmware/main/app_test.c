// Hardware test. Mic straight to the DACs, level on the screen and in the
// log, raw I2S words dumped so a dead, miswired or stuck mic is
// diagnosable. Encoder page when the knob moves. Tap BOOT or the encoder
// to add the 440 Hz sweep tone (fx_sine.c). While nothing happens the
// screen cycles a grid / text / animation page to exercise the OLED.
#include "app.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include "voice.h"
#include "input.h"
#include "pins.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "test";
float fx_sine_db(void);
void  fx_sine_set_tone(bool on);
static bool tone;
static int enc_count, enc_last_dir, enc_presses;

static const char *diagnose(int32_t l, int32_t r, int nzl, int nzr, int n)
{
    if (nzl == 0 && nzr == 0) return "no data: check SD->GPIO10, VDD 3V3, SCK->7, WS->8";
    if (nzl == 0 && nzr > 0) return "data in RIGHT slot: tie L/R to GND";
    if ((uint32_t)l == 0xFFFFFFFF || (l >> 8) == 0x7FFFFF || (l >> 8) == -0x800000) return "line stuck/floating: check SD wire and GND";
    if (nzl > 0 && nzl < n / 2) return "intermittent data: loose SD or clock wire";
    return "mic data OK";
}

// Are the I2S clocks really toggling on the pins? Read them back a few
// hundred times and count edges.
static int edges(int pin)
{
    gpio_input_enable(pin);
    int last = gpio_get_level(pin), n = 0;
    int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 2000) { int l = gpio_get_level(pin); if (l != last) n++; last = l; }   // 2 ms: WS gives ~128 edges
    return n;
}

// Is anything driving SD? Pull it up, then down, and see if the data
// follows the pull. A working mic overrides the weak pull in both cases.
static const char *probe_sd(void)
{
    int32_t l, r; int nzl, nzr;
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(30));
    audio_get_raw(&l, &r, &nzl, &nzr);
    int32_t lu = l; int level_up = gpio_get_level(PIN_I2S_DIN);
    bool up_high = (uint32_t)l > 0xF0000000u;
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLDOWN_ONLY);
    vTaskDelay(pdMS_TO_TICKS(30));
    audio_get_raw(&l, &r, &nzl, &nzr);
    int level_down = gpio_get_level(PIN_I2S_DIN);
    bool down_low = l == 0 && r == 0;
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_FLOATING);
    ESP_LOGI(TAG, "SD probe: with pull-up word 0x%08lX pin=%d, with pull-down word 0x%08lX pin=%d",
             (unsigned long)lu, level_up, (unsigned long)l, level_down);
    if (up_high && down_low) return "SD follows the pull: NOTHING drives it (mic has no VDD, no clocks, or SD is not on GPIO10)";
    if (!up_high && !down_low) return "SD is driven by something";
    return up_high ? "SD stuck low-ish: is it shorted to GND?" : "SD stuck high-ish: is it shorted to 3V3?";
}

static void page_grid(int frame)
{
    oled_rect(0, 0, OLED_W, OLED_H, false);
    for (int x = 0; x < OLED_W; x += 8) oled_vline(x, 0, OLED_H - 1, true);
    for (int y = 0; y < OLED_H; y += 8) oled_hline(0, OLED_W - 1, y, true);
    bool on = (frame / 12) & 1;
    oled_rect(0, 0, 6, 6, on); oled_rect(OLED_W - 6, 0, 6, 6, on);
    oled_rect(0, OLED_H - 6, 6, 6, on); oled_rect(OLED_W - 6, OLED_H - 6, 6, 6, on);
    oled_text(40, 28, "128 x 64 grid");
}

static void page_text(void)
{
    oled_text2(4, 2, "OLED OK");
    oled_text(4, 20, "ABCDEFGHIJKLMNOPQRSTU");
    oled_text(4, 29, "abcdefghijklmnopqrstu");
    oled_text(4, 38, "0123456789 !\"#$%&'()*+");
    oled_text(4, 50, tone ? "tone on (tap: off)" : "tap BOOT for a tone");
}

static void page_anim(int frame)
{
    int x = frame % 220, y = frame % 92;
    if (x > 110) x = 220 - x;
    if (y > 46) y = 92 - y;
    oled_circle(9 + x, 9 + y, 6, true);
    float a = frame * 0.12f;
    oled_line(100, 24, 100 + (int)(16 * cosf(a)), 24 + (int)(16 * sinf(a)), true);
    oled_circle(100, 24, 17, false);
}

static void page_encoder(const input_ev_t *in)
{
    char line[32];
    oled_text2(0, 0, "ENCODER");
    snprintf(line, sizeof line, "%+d", enc_count);
    oled_text2(OLED_W - 12 * strlen(line), 0, line);
    snprintf(line, sizeof line, "A=%d  B=%d  SW=%d  BOOT=%d", gpio_get_level(PIN_ENC_A), gpio_get_level(PIN_ENC_B),
             gpio_get_level(PIN_ENC_SW), gpio_get_level(0));
    oled_text(0, 20, line);
    snprintf(line, sizeof line, "presses %d  %s", enc_presses, in->pressed ? "held" : "");
    oled_text(0, 30, line);
    if (enc_last_dir) oled_text2(52, 42, enc_last_dir > 0 ? ">>" : "<<");
    oled_text(0, 57, "turn: count, push: tone");
}

static void page_mic(const voice_t *v, const char *diag)
{
    char line[32], nm[5];
    oled_text2(0, 0, "MIC");
    snprintf(line, sizeof line, "%4.0f", v->db > -99 ? v->db : -99);
    oled_text2(44, 0, line);
    oled_text(100, 7, "dBFS");
    int bar = (int)((v->db + 60) / 60 * 126);
    oled_rect(0, 17, OLED_W, 7, false);
    oled_rect(1, 18, bar > 0 ? (bar > 126 ? 126 : bar) : 0, 5, true);
    for (int d = -50; d <= 0; d += 10) oled_pixel(1 + (d + 60) * 126 / 60, 24, true);
    if (v->voiced) snprintf(line, sizeof line, "%s %.0f Hz", note_name(v->note, nm), v->freq);
    else snprintf(line, sizeof line, "%s", v->gate ? "voice" : "quiet");
    oled_text(0, 27, line);
    oled_text(0, 57, diag);
    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    for (int x = 1; x < OLED_W; x++) oled_pixel(x, 45 - wave[x] * 9 / 128, true);
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
    input_init();
    ESP_LOGI(TAG, "mic test: mic -> both DACs, %d Hz; tap BOOT or the encoder for the sweep tone; OLED %s",
             CONFIG_KIT_SAMPLE_RATE, oled_present() ? "found" : "not found, rescanning");
    ESP_LOGI(TAG, "clock check: BCLK GPIO%d %s, WS GPIO%d %s", PIN_I2S_BCLK, edges(PIN_I2S_BCLK) > 10 ? "toggling" : "NOT toggling",
             PIN_I2S_WS, edges(PIN_I2S_WS) > 2 ? "toggling" : "NOT toggling");
    int frame = 0, restarts = 0;
    int64_t last_log = 0, last_scan = 0, enc_t = 0, stuck_since = 0;
    char diag[64] = "";
    while (1) {
        int64_t now = esp_timer_get_time();
        input_ev_t in;
        input_poll(&in);
        if (in.tap) { tone = !tone; fx_sine_set_tone(tone); enc_presses++; ESP_LOGI(TAG, "push: tone %s", tone ? "on" : "off"); }
        if (in.enc) {
            enc_count += in.enc; enc_last_dir = in.enc; enc_t = now;
            ESP_LOGI(TAG, "encoder %+d -> %d  (A=%d B=%d SW=%d)", in.enc, enc_count, gpio_get_level(PIN_ENC_A), gpio_get_level(PIN_ENC_B), gpio_get_level(PIN_ENC_SW));
        }
        if (in.pressed) enc_t = now;
        if (now - last_scan > 2000000 && !oled_present()) { last_scan = now; i2c_rescan(bus); }

        voice_t v; char nm[5];
        audio_get_voice(&v);
        int32_t rl, rr; int nzl, nzr;
        audio_get_raw(&rl, &rr, &nzl, &nzr);
        snprintf(diag, sizeof diag, "%s", diagnose(rl, rr, nzl, nzr, 64));

        // stuck mic: all ones or all zeros for a while although the clocks run
        bool stuck = (uint32_t)rl == 0xFFFFFFFFu || (rl == 0 && nzl == 0);
        if (!stuck) stuck_since = 0;
        else if (!stuck_since) stuck_since = now;
        else if (now - stuck_since > 700000) {
            stuck_since = 0;
            ESP_LOGW(TAG, "mic stuck at 0x%08lX for 0.7 s; BCLK %s, WS %s; restarting I2S (restart #%d)", (unsigned long)rl,
                     edges(PIN_I2S_BCLK) > 10 ? "running" : "STOPPED", edges(PIN_I2S_WS) > 2 ? "running" : "STOPPED", ++restarts);
            ESP_LOGW(TAG, "%s", probe_sd());
            static const char *held[3] = { "free", "SHORTED LOW", "SHORTED HIGH" };
            ESP_LOGW(TAG, "pin check: BCLK GPIO%d %s, WS GPIO%d %s, DOUT GPIO%d %s", PIN_I2S_BCLK, held[audio_pin_held(PIN_I2S_BCLK)],
                     PIN_I2S_WS, held[audio_pin_held(PIN_I2S_WS)], PIN_I2S_DOUT, held[audio_pin_held(PIN_I2S_DOUT)]);
            audio_restart_i2s();
        }

        if (now - last_log > 500000) {
            last_log = now;
            int bar = (int)((v.db + 60) / 60 * 30);
            char line[40];
            snprintf(line, sizeof line, "|%-30.*s|", bar > 0 ? bar : 0, "##############################");
            int mn, mx, dc;
            audio_get_raw_stats(&mn, &mx, &dc);
            ESP_LOGI(TAG, "mic %6.1f dBFS %s %s %-3s | raw L 0x%08lX R 0x%08lX nz %d/%d | 24-bit min %d max %d dc %d | %s | cpu %2.0f%%%s",
                     v.db, line, v.gate ? "VOICE" : "quiet", v.voiced ? note_name(v.note, nm) : "---",
                     (unsigned long)rl, (unsigned long)rr, nzl, nzr, mn, mx, dc, diag, audio_cpu_load() * 100, tone ? " tone" : "");
        }
        if (oled_present()) {
            oled_clear();
            int phase = (frame / 75) % 4;
            if (v.gate || nzl > 0 || (frame / 75) % 4 == 3) phase = 3;   // any mic data: stay on the mic page
            if (now - enc_t < 3000000) phase = 4;                          // encoder activity wins for 3 s
            if (phase == 4) page_encoder(&in);
            else if (phase == 0) page_grid(frame);
            else if (phase == 1) page_text();
            else if (phase == 2) page_anim(frame);
            else page_mic(&v, diag);
            oled_flush();
        }
        frame++;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
