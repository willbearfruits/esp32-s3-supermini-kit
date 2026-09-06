// Looper UI: one button, one screen.
//   tap            arm a take on the current page (tap again to cancel)
//   hold 0.5-2.5 s next page (fires on release)
//   hold 2.5 s     clear the song
#include "app.h"
#include "looper.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include "drums.h"
#include "pins.h"
#include "voice.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "looper-ui";
#define PIN_BOOT 0

static void draw_drums(const looper_ui_t *u, int y0, int h)
{
    const uint8_t *pat = looper_drum_pattern();
    int steps = u->steps > 0 ? u->steps : 16;
    int rh = h / 3;
    for (int t = 0; t < DRUM_N; t++) {
        int row = DRUM_N - 1 - t;                       // hat on top, kick at the bottom
        int y = y0 + row * rh;
        for (int s = 0; s < steps; s++) {
            int x0 = s * OLED_W / steps, x1 = (s + 1) * OLED_W / steps;
            int vel = pat[s * DRUM_N + t];
            if (vel) {
                int hh = 1 + vel * (rh - 2) / 127;
                oled_rect(x0, y + rh - 1 - hh, x1 - x0 - 1 > 0 ? x1 - x0 - 1 : 1, hh, true);
            }
        }
        for (int b = 0; b <= steps; b += 16) oled_pixel(b * OLED_W / steps, y + rh - 1, true);
    }
}

static void draw_seq(const looper_ui_t *u, int y0, int h)
{
    const uint8_t *seq = looper_seq(u->page);
    int steps = u->steps > 0 ? u->steps : 16;
    int lo = 127, hi = 0;
    for (int s = 0; s < steps; s++) {
        int n = seq[s] & 0x7F;
        if (!n) continue;
        if (n < lo) lo = n;
        if (n > hi) hi = n;
    }
    if (hi - lo < 12) { int c = (hi + lo) / 2; lo = c - 6; hi = c + 6; }
    for (int s = 0; s < steps; s++) {
        int n = seq[s] & 0x7F;
        if (!n) continue;
        int x0 = s * OLED_W / steps, x1 = (s + 1) * OLED_W / steps;
        int y = y0 + h - 1 - (n - lo) * (h - 2) / (hi - lo);
        if (seq[s] & SEQ_ATTACK) oled_rect(x0, y, x1 - x0 > 1 ? x1 - x0 - 1 : 1, y0 + h - y, true);
        else oled_hline(x0, x1 - 1, y, true);
    }
}

static void draw_wave(int y0, int h)
{
    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    int cy = y0 + h / 2, amp = h / 2 - 1;
    int prev = cy - wave[0] * amp / 128;
    for (int x = 1; x < OLED_W; x++) {
        int y = cy - wave[x] * amp / 128;
        oled_vline(x, prev, y, true);
        prev = y;
    }
}

static void draw(const looper_ui_t *u, bool blink, int hold_ms)
{
    char line[32], nm[5];
    oled_clear();

    snprintf(line, sizeof line, "%d %s", u->page + 1, looper_page_name(u->page));
    oled_text(0, 0, line);
    char arm[8];
    const char *st = u->rec ? (blink ? "REC" : "   ") : (u->has[u->page] ? "PLAY" : "LIVE");
    if (u->armed) { snprintf(arm, sizeof arm, "IN %d", u->beats_to_go); st = arm; }
    if (hold_ms > 500) st = hold_ms > 2500 ? "CLEAR" : "NEXT>";
    oled_text(OLED_W - 6 * strlen(st), 0, st);
    oled_hline(0, OLED_W - 1, 8, true);

    snprintf(line, sizeof line, "%3.0fbpm %db %-5s %2.0f%%", u->bpm, u->bars, u->key, audio_cpu_load() * 100);
    oled_text(0, 10, line);

    if (u->msg[0]) snprintf(line, sizeof line, "%s", u->msg);
    else if (u->page == PG_DRUMS) {
        if (u->last_drum >= 0) snprintf(line, sizeof line, "%-5s lo%.2f hi%.2f", drums_name(u->last_drum), u->drum_lo, u->drum_hi);
        else snprintf(line, sizeof line, "beatbox: b / k / ts");
    } else if (u->live_note >= 0) snprintf(line, sizeof line, "> %s", note_name(u->live_note, nm));
    else snprintf(line, sizeof line, "%s", u->page == PG_VOCAL ? "sing" : "hum a note");
    oled_text(0, 19, line);

    // loop position
    oled_rect(0, 28, OLED_W, 4, false);
    {
        int w = (u->step + 1) * (OLED_W - 2) / u->steps;
        oled_rect(1, 29, w, 2, true);
        for (int b = 1; b < u->bars; b++) oled_vline(b * OLED_W / u->bars, 28, 31, true);
    }

    const int y0 = 34, h = OLED_H - y0;
    if (u->page == PG_VOCAL) draw_wave(y0, h);
    else if (u->page == PG_DRUMS) draw_drums(u, y0, h);
    else draw_seq(u, y0, h);
    oled_vline(u->step * OLED_W / u->steps, y0 - 2, y0 - 1, true);
    oled_flush();
}

void app_looper_run(void)
{
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_BOOT, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&in));

    bool was = false, cleared = false;
    int64_t t0 = 0, last_draw = 0, last_log = 0;
    while (1) {
        int64_t now = esp_timer_get_time();
        bool pressed = gpio_get_level(PIN_BOOT) == 0;
        int hold_ms = pressed ? (int)((now - t0) / 1000) : 0;
        if (pressed && !was) { t0 = now; cleared = false; hold_ms = 0; }
        if (pressed && !cleared && hold_ms >= 2500) { looper_event(LP_CLEAR); cleared = true; }
        if (!pressed && was && !cleared) {
            int ms = (int)((now - t0) / 1000);
            if (ms >= 40 && ms < 500) looper_event(LP_SHORT);
            else if (ms >= 500) looper_event(LP_LONG);
        }
        was = pressed;

        if (oled_present() && now - last_draw > 50000) {
            looper_ui_t u;
            looper_get_ui(&u);
            draw(&u, (now / 250000) & 1, hold_ms);
            last_draw = now;
        }
        if (now - last_log > 1000000) {
            last_log = now;
            looper_ui_t u; voice_t v; char nm[5];
            looper_get_ui(&u);
            audio_get_voice(&v);
            ESP_LOGI(TAG, "%-6s %s %-6s | %s %5.1f dB floor %5.1f %s conf %.2f | cpu %2.0f%% | %s",
                     looper_page_name(u.page), u.rec ? "REC " : (u.armed ? "arm " : "play"), u.key,
                     v.gate ? "VOICE" : "quiet", v.db, voice_noise_db(), v.voiced ? note_name(v.note, nm) : "---", v.conf,
                     audio_cpu_load() * 100, u.msg);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
