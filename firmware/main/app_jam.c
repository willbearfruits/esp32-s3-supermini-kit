// JAM UI task: three pages.
//   PATTERN  step grid of the selected channel. Encoder turn moves the cursor
//            along the steps, joystick flicks move it (left/right step,
//            up/down row), encoder tap toggles the cell, encoder hold clears
//            the channel, 3 s clears everything. On MIC, encoder tap starts
//            and stops sample recording. Joystick click toggles PERFORM: the
//            stick then plays the channel (scratch on SAMPLE, bend and filter
//            on the synths, effect sends on PAD and MIC).
//   MIX      one row per channel: vol, pan, rev, dly. Flick left/right picks
//            the field, encoder turn adjusts it, encoder tap mutes.
//   SETUP    bpm, root, scale, drum template, swing, kit, presets. Flick
//            up/down or BOOT tap picks the field, encoder turn adjusts it.
// BOOT tap: next channel (PATTERN, MIX) or next field (SETUP). BOOT hold:
// next page.
#include "app.h"
#include "jam.h"
#include "input.h"
#include "oled.h"
#include "audio.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "jam-ui";
extern float jam_prof[8];
enum { PG_PATTERN, PG_MIX, PG_SETUP, PG_N };
static int page, ch, cur_step, cur_row, mix_field, setup_field;
static bool perform;

static void draw_pattern(const jam_ui_t *u)
{
    char line[64];
    int bar = cur_step / 16;
    snprintf(line, sizeof line, "%-6s b%d %3d %s%s%s", jam_ch_name(u->sel), bar + 1, u->bpm, u->scale,
             u->perform ? " PERF" : "", u->rec ? " REC" : "");
    oled_text(0, 0, line);
    if (u->sel == CH_MIC) {
        int bar_px = (int)((u->mic_db + 60) / 60 * 120); if (bar_px < 0) bar_px = 0; if (bar_px > 120) bar_px = 120;
        oled_text(0, 16, "live mic on this channel");
        oled_rect(4, 28, bar_px, 8, true); oled_rect(4, 28, 120, 8, false);
        oled_text(0, 42, u->rec ? "recording... tap to stop" : "tap: record sample");
        if (u->sample_ok) { snprintf(line, sizeof line, "sample %.2f s", u->sample_s); oled_text(0, 52, line); }
        return;
    }
    int rows = u->rows, h = rows > 0 ? 48 / rows : 8; if (h > 12) h = 12;
    int top = 9 + (48 - h * rows) / 2;
    int col0 = bar * 16;
    // playhead on the row above the grid, when it is in this bar
    if (u->step / 16 == bar) oled_rect(8 + (u->step % 16) * 7, top - 2, 6, 1, true);
    for (int r = 0; r < rows; r++) {
        int y = top + r * h;
        if (u->root_row[r]) oled_rect(0, y + h / 2 - 1, 3, 2, true);
        if (u->sel == CH_DRUMS) oled_text(1, y + (h - 7) / 2, u->row_label[r]);
        for (int cidx = 0; cidx < 16; cidx++) {
            int s = col0 + cidx, x = 8 + cidx * 7;
            uint8_t g = u->grid[r][s];
            if (g & 1) oled_rect(x + 1, y + 1, 5, h - 2, true);
            else if (g & 0x80) oled_rect(x + 3, y + h / 2, 1, 1, true);
            else if (cidx % 4 == 0) oled_pixel(x + 3, y + h / 2, true);
        }
    }
    // cursor
    int cx = 8 + (cur_step % 16) * 7, cy = top + (rows - 1 - cur_row) * h;
    oled_rect(cx, cy, 7, h, false);
    snprintf(line, sizeof line, "%02d", cur_step + 1);
    oled_text(116, 57, line);
}

static void draw_mix(const jam_ui_t *u)
{
    char line[64];
    static const char *F[MX_N] = { "vol", "pan", "rev", "dly" };
    snprintf(line, sizeof line, "MIX   %s", F[mix_field]);
    oled_text(0, 0, line);
    for (int i = 0; i < CH_N; i++) {
        int y = 9 + i * 9;
        if (i == ch) { oled_rect(0, y - 1, 30, 9, true); oled_text_c(1, y, jam_ch_name(i), false); }
        else oled_text(1, y, jam_ch_name(i));
        if (u->mute[i]) oled_text(32, y, "M");
        int x = 40;
        for (int f = 0; f < MX_N; f++) {
            float v = u->mix[i][f]; int w = 18;
            if (f == MX_PAN) { oled_rect(x + w / 2, y + 1, 1, 6, true); int px = x + w / 2 + (int)(v * w / 2); oled_rect(px - 1, y + 2, 3, 4, true); }
            else { oled_rect(x, y + 5, w, 2, false); oled_rect(x, y + 2, (int)(v * w), 5, true); }
            if (i == ch && f == mix_field) oled_rect(x, y + 8, w, 1, true);
            x += w + 4;
        }
    }
}

static void draw_setup(const jam_ui_t *u)
{
    char line[64];
    oled_text(0, 0, "SETUP");
    int first = setup_field > 5 ? setup_field - 5 : 0;
    for (int k = 0; k < 6 && first + k < P_N; k++) {
        int p = first + k, y = 10 + k * 9;
        snprintf(line, sizeof line, "%-6s %s", jam_param_name(p), u->param[p]);
        if (p == setup_field) { oled_rect(0, y - 1, 128, 9, true); oled_text_c(1, y, line, false); }
        else oled_text(1, y, line);
    }
}

void app_jam_run(i2c_master_bus_handle_t bus)
{
    (void)bus;
    input_init();
    ESP_LOGI(TAG, "JAM: BOOT tap = channel, BOOT hold = page, encoder = cursor/value, tap = toggle, joystick click = perform");
    bool turned = false; int64_t last_log = 0;
    jam_ui_t u; jam_get_ui(&u);
    while (1) {
        input_ev_t in; input_poll(&in);
        int rows = u.rows;
        // --- navigation & values ---
        if (in.enc) {
            if (page == PG_PATTERN) { if (ch != CH_MIC) cur_step = (cur_step + in.enc + JAM_STEPS) % JAM_STEPS; }
            else if (page == PG_MIX) jam_cmd(CMD_MIX, ch, mix_field, in.enc);
            else jam_cmd(CMD_PARAM, 0, setup_field, in.enc);
            if (in.pressed) turned = true;
        }
        if (in.has_joy && !perform) {
            if (in.flick_r) { if (page == PG_PATTERN) cur_step = (cur_step + 1) % JAM_STEPS; else if (page == PG_MIX) mix_field = (mix_field + 1) % MX_N; }
            if (in.flick_l) { if (page == PG_PATTERN) cur_step = (cur_step + JAM_STEPS - 1) % JAM_STEPS; else if (page == PG_MIX) mix_field = (mix_field + MX_N - 1) % MX_N; }
            if (in.flick_u) { if (page == PG_PATTERN && rows) cur_row = (cur_row + 1) % rows; else if (page == PG_SETUP) setup_field = (setup_field + P_N - 1) % P_N; else if (page == PG_MIX) { ch = (ch + CH_N - 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); } }
            if (in.flick_d) { if (page == PG_PATTERN && rows) cur_row = (cur_row + rows - 1) % rows; else if (page == PG_SETUP) setup_field = (setup_field + 1) % P_N; else if (page == PG_MIX) { ch = (ch + 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); } }
        }
        if (in.has_joy && in.joy_click && page == PG_PATTERN) { perform = !perform; jam_cmd(CMD_PERFORM, ch, perform, 0); ESP_LOGI(TAG, "perform %s", perform ? "on" : "off"); }
        if (perform) jam_expr(in.jx, in.jy);
        // --- buttons ---
        if (in.tap || in.hold || in.longp) {
            if (turned) turned = false;
            else if (in.from_boot) {
                if (in.tap) {
                    if (page == PG_SETUP) setup_field = (setup_field + 1) % P_N;
                    else { ch = (ch + 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); cur_row = 0; }
                } else if (in.hold) { page = (page + 1) % PG_N; ESP_LOGI(TAG, "page %d", page); }
            } else if (in.longp) { jam_cmd(CMD_CLEAR_ALL, 0, 0, 0); ESP_LOGI(TAG, "clear all"); }
            else if (page == PG_PATTERN) {
                if (ch == CH_MIC) { if (in.tap) jam_cmd(CMD_REC, ch, !u.rec, 0); }
                else if (in.tap) jam_cmd(CMD_TOGGLE, ch, cur_step, cur_row);
                else if (in.hold) { jam_cmd(CMD_CLEAR_CH, ch, 0, 0); ESP_LOGI(TAG, "clear %s", jam_ch_name(ch)); }
            } else if (page == PG_MIX) { if (in.tap) jam_cmd(CMD_MUTE, ch, 0, 0); }
        }
        if (!in.pressed) turned = false;
        if (rows && cur_row >= rows) cur_row = rows - 1;

        jam_get_ui(&u);
        if (oled_present()) {
            oled_clear();
            if (page == PG_PATTERN) draw_pattern(&u); else if (page == PG_MIX) draw_mix(&u); else draw_setup(&u);
            oled_flush();
        }
        int64_t now = esp_timer_get_time();
        if (now - last_log > 3000000) {
            last_log = now;
            const float blk = 64.0f * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1e6f / CONFIG_KIT_SAMPLE_RATE;
            ESP_LOGI(TAG, "%s %s | %d bpm %s | step %2d cursor %2d/%d | cpu %2.0f%% (seq %.0f drums %.0f bass %.0f lead %.0f pad %.0f smp %.0f mix %.0f, total %.0f)",
                     page == PG_PATTERN ? "PATTERN" : page == PG_MIX ? "MIX" : "SETUP", jam_ch_name(ch), u.bpm, u.scale, u.step, cur_step, cur_row,
                     audio_cpu_load() * 100, jam_prof[0] / blk * 100, jam_prof[1] / blk * 100, jam_prof[2] / blk * 100, jam_prof[3] / blk * 100,
                     jam_prof[4] / blk * 100, jam_prof[5] / blk * 100, jam_prof[6] / blk * 100, jam_prof[7] / blk * 100);
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
