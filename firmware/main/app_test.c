// Hardware test, mic and DAC, no screen or controls needed. The mic goes
// through the chosen effect (mic only, delay, reverb, beat repeat) to the
// DACs; the serial log (and the screen, if there is one) shows level, note,
// a waveform and a one-line diagnosis from the raw I2S words. The first
// seconds after boot show why the chip reset and how many times, so a
// brownout loop is visible without a serial cable. BOOT (or the encoder
// push): tap = next effect, hold half a second = next preset. Turning the
// encoder shows its page for two seconds.
#include "app.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include "voice.h"
#include "input.h"
#include "pins.h"
#include "fx.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "sdkconfig.h"

static const char *TAG = "test";
#define AUTO_TONE_S 4   // seconds of test tone after boot
float fx_sine_db(void);
void  fx_sine_set_tone(bool on);
static bool tone;
static int enc_count, enc_last_dir, enc_presses;
static int fx_cur, preset_cur[16];
static const char *preset_name = "";

static void set_preset(int idx)
{
    const fx_t *fx = fx_list[fx_cur];
    if (!fx->preset) { preset_name = ""; return; }
    const char *nm = fx->preset(idx);
    if (!nm) { idx = 0; nm = fx->preset(0); }
    preset_cur[fx_cur] = idx; preset_name = nm ? nm : "";
    tone = fx_cur == 0 && idx == 1;
    ESP_LOGI(TAG, "%s preset %d: %s", fx->name, idx + 1, preset_name);
}

static void set_fx(int idx)
{
    fx_cur = idx % fx_count;
    audio_set_mode(fx_cur);
    ESP_LOGI(TAG, "effect %d/%d: %s  (tap: next effect, hold: next preset)", fx_cur + 1, fx_count, fx_list[fx_cur]->name);
    set_preset(preset_cur[fx_cur]);
}
static RTC_NOINIT_ATTR uint32_t boot_count;

static const char *reset_name(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "int watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB:      return "usb";
    case ESP_RST_JTAG:     return "jtag";
    default:               return "other";
    }
}

static const char *diagnose(int nzl, int nzr, int livel, int liver, int slot, int n)
{
    if (nzl == 0 && nzr == 0) return "no data: SD->10? VDD? SCK->7 WS->8?";
    if (livel == 0 && liver == 0) return "line stuck: SD wire? mic power?";
    int live = slot ? liver : livel;
    if (live < n / 2) return "intermittent: loose wire";
    if (slot) return "mic OK, RIGHT slot (L/R pin high)";
    return "mic data OK";
}

static int edges(int pin)
{
    gpio_input_enable(pin);
    int last = gpio_get_level(pin), n = 0;
    int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 2000) { int l = gpio_get_level(pin); if (l != last) n++; last = l; }
    return n;
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
    oled_text(0, 57, "tap: effect  hold: preset");
}

static void page_mic(const voice_t *v, const char *diag, bool show_boot)
{
    char line[32], nm[5];
    if (show_boot) {
        snprintf(line, sizeof line, "boot %lu: %s", (unsigned long)boot_count, reset_name());
        oled_rect(0, 0, OLED_W, 9, true);
        oled_text_c(1, 1, line, false);
    } else {
        oled_text2(0, 0, "MIC");
        snprintf(line, sizeof line, "%4.0f", v->db > -99 ? v->db : -99);
        oled_text2(44, 0, line);
        oled_text(100, 7, "dBFS");
    }
    int bar = (int)((v->db + 60) / 60 * 126);
    oled_rect(0, 17, OLED_W, 7, false);
    oled_rect(1, 18, bar > 0 ? (bar > 126 ? 126 : bar) : 0, 5, true);
    for (int d = -50; d <= 0; d += 10) oled_pixel(1 + (d + 60) * 126 / 60, 24, true);
    if (v->voiced) snprintf(line, sizeof line, "%s %.0f Hz", note_name(v->note, nm), v->freq);
    else snprintf(line, sizeof line, "%s%s", v->gate ? "voice" : "quiet", tone ? "  tone on" : "");
    oled_text(0, 27, line);
    snprintf(line, sizeof line, "%s: %s", fx_list[fx_cur]->name, preset_name);
    oled_text(0, 49, line);
    oled_text(0, 57, diag);
    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    for (int x = 1; x < OLED_W; x++) oled_pixel(x, 41 - wave[x] * 5 / 128, true);   // rows 36..46
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
    if (esp_reset_reason() == ESP_RST_POWERON || boot_count > 100000) boot_count = 0;
    boot_count++;
    ESP_LOGI(TAG, "boot %lu, reset reason: %s", (unsigned long)boot_count, reset_name());
    input_init();
    ESP_LOGI(TAG, "mic test: mic -> both DACs, %d Hz; OLED %s", CONFIG_KIT_SAMPLE_RATE, oled_present() ? "found" : "not found, rescanning");
    ESP_LOGI(TAG, "clock check: BCLK GPIO%d %s, WS GPIO%d %s", PIN_I2S_BCLK, edges(PIN_I2S_BCLK) > 10 ? "toggling" : "NOT toggling",
             PIN_I2S_WS, edges(PIN_I2S_WS) > 2 ? "toggling" : "NOT toggling");
    int64_t t_start = esp_timer_get_time(), last_log = 0, last_scan = 0, enc_t = 0, stuck_since = 0;
    int restarts = 0;
    char diag[64] = "";
    // DAC check without touching anything: the 440 Hz tone rises from -50 dB
    // for the first seconds after boot, then the mic passthrough is on its own.
    bool auto_tone = true;
    preset_cur[0] = 1;                 // start on the tone preset of the mic mode
    set_fx(0);
    ESP_LOGI(TAG, "DAC check: 440 Hz tone rising for %d s, then mic only", AUTO_TONE_S);
    while (1) {
        int64_t now = esp_timer_get_time();
        input_ev_t in;
        input_poll(&in);
        if (auto_tone && now - t_start > AUTO_TONE_S * 1000000LL) { auto_tone = false; if (fx_cur == 0) set_preset(0); }
        if (in.tap)  { auto_tone = false; enc_presses++; set_fx(fx_cur + 1); }
        if (in.hold) { auto_tone = false; enc_presses++; set_preset(preset_cur[fx_cur] + 1); }
        if (in.enc) {
            enc_count += in.enc; enc_last_dir = in.enc; enc_t = now;
            ESP_LOGI(TAG, "encoder %+d -> %d  (A=%d B=%d SW=%d)", in.enc, enc_count, gpio_get_level(PIN_ENC_A), gpio_get_level(PIN_ENC_B), gpio_get_level(PIN_ENC_SW));
        }
        if (now - last_scan > 2000000 && !oled_present()) { last_scan = now; i2c_rescan(bus); }

        voice_t v; char nm[5];
        audio_get_voice(&v);
        int32_t rl, rr; int nzl, nzr, livel, liver;
        audio_get_raw(&rl, &rr, &nzl, &nzr);
        audio_get_raw_live(&livel, &liver);
        int slot = audio_mic_slot();
        snprintf(diag, sizeof diag, "%s", diagnose(nzl, nzr, livel, liver, slot, 64));

        bool stuck = livel == 0 && liver == 0;
        if (!stuck) stuck_since = 0;
        else if (!stuck_since) stuck_since = now;
        else if (now - stuck_since > 1500000) {
            stuck_since = 0;
            ESP_LOGW(TAG, "mic stuck at 0x%08lX; BCLK %s, WS %s (restart #%d)", (unsigned long)rl,
                     edges(PIN_I2S_BCLK) > 10 ? "running" : "STOPPED", edges(PIN_I2S_WS) > 2 ? "running" : "STOPPED", ++restarts);
            static const char *held[3] = { "free", "held LOW", "held HIGH" };
            ESP_LOGW(TAG, "pin check: WS GPIO%d %s, SD GPIO%d %s", PIN_I2S_WS, held[audio_pin_held(PIN_I2S_WS)], PIN_I2S_DIN, held[audio_pin_held(PIN_I2S_DIN)]);
        }

        if (now - last_log > 500000) {
            last_log = now;
            int bar = (int)((v.db + 60) / 60 * 30), mn, mx, dc;
            char line[40];
            snprintf(line, sizeof line, "|%-30.*s|", bar > 0 ? bar : 0, "##############################");
            audio_get_raw_stats(&mn, &mx, &dc);
            char st[40]; fx_list[fx_cur]->status(st, sizeof st);
            ESP_LOGI(TAG, "mic %6.1f dBFS %s %s %-3s | raw L 0x%08lX R 0x%08lX live %d/%d slot %c | 24-bit min %d max %d dc %d | %s | %s: %s | cpu %2.0f%%",
                     v.db, line, v.gate ? "VOICE" : "quiet", v.voiced ? note_name(v.note, nm) : "---",
                     (unsigned long)rl, (unsigned long)rr, livel, liver, slot ? 'R' : 'L', mn, mx, dc, diag, fx_list[fx_cur]->name, st, audio_cpu_load() * 100);
        }
        if (oled_present()) {
            oled_clear();
            if (now - enc_t < 2000000) page_encoder(&in);
            else page_mic(&v, diag, now - t_start < 4000000);
            oled_flush();
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
