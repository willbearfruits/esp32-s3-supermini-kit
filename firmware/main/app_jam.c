// JAM UI task: four pages, driven by the encoder and the joystick only
// (BOOT is inside the case). Everywhere: encoder hold 0.5 s = next page,
// held 3 s = clear the channel.
//   PATTERN  step grid of the selected channel. Encoder turn moves the
//            cursor along the steps, joystick flicks move it (left/right
//            step, up/down row), encoder tap toggles the cell. On MIC the
//            tap starts and stops sample recording. Push + turn = channel.
//            Push + flick up/down = bank. Joystick click = perform mode.
//   MIX      channel strips. Flick left/right = channel, up/down = field,
//            encoder turn adjusts, tap mutes.
//   SONG     arrangement of banks. Flick left/right = slot, encoder turn =
//            bank of the slot (past H wraps to end), push + turn = repeats,
//            tap = play the song from that slot / stop.
//   SETUP    list of settings. Flick up/down or push + turn = field, encoder
//            turn adjusts, tap fires COPY and CLEAR ALL.
#include "app.h"
#include "jam.h"
#include "input.h"
#include "oled.h"
#include "audio.h"
#include "dsp.h"
#include "mix.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <sys/stat.h>

static const char *TAG = "jam-ui";
extern float jam_prof[8];
enum { PG_PATTERN, PG_MIX, PG_SONG, PG_SETUP, PG_N };
static int page, ch, cur_step, cur_row, mix_field, setup_field, song_slot, perform;
static unsigned frame;

// ---- persistence: /storage/jam/state.bin + sample.raw (int16) -----------------
#define JAM_DIR   "/storage/jam"
#define STATE_F   JAM_DIR "/state.bin"
#define SAMPLE_F  JAM_DIR "/sample.raw"
static bool mounted;
static jam_state_t st_buf;

static bool storage_mount(void)
{
    static wl_handle_t wl = WL_INVALID_HANDLE;
    esp_vfs_fat_mount_config_t mc = { .max_files = 4, .format_if_mount_failed = true, .allocation_unit_size = 4096 };
    esp_err_t e = esp_vfs_fat_spiflash_mount_rw_wl("/storage", "storage", &mc, &wl);
    if (e != ESP_OK) { ESP_LOGE("jam-ui", "storage mount failed: %s", esp_err_to_name(e)); return false; }
    mkdir(JAM_DIR, 0777);
    return true;
}

static void save_state(void)
{
    if (!mounted) return;
    jam_get_state(&st_buf);
    FILE *f = fopen(STATE_F, "wb");
    if (!f) { ESP_LOGW("jam-ui", "cannot write %s", STATE_F); return; }
    fwrite(&st_buf, sizeof st_buf, 1, f); fclose(f);
    ESP_LOGI("jam-ui", "saved state");
}

static void save_sample(void)
{
    if (!mounted) return;
    int max; float *buf = jam_sample_buf(&max);
    jam_get_state(&st_buf);
    int len = st_buf.smp_len; if (len <= 0 || !buf) return;
    FILE *f = fopen(SAMPLE_F, "wb");
    if (!f) return;
    static int16_t chunk[1024];
    for (int i = 0; i < len; i += 1024) {
        int n = len - i < 1024 ? len - i : 1024;
        for (int k = 0; k < n; k++) { float v = buf[i + k] * 32767.0f; chunk[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
        fwrite(chunk, sizeof(int16_t), n, f);
    }
    fclose(f);
    ESP_LOGI("jam-ui", "saved sample (%d samples)", len);
}

static void load_all(void)
{
    if (!mounted) return;
    FILE *f = fopen(STATE_F, "rb");
    if (!f) { ESP_LOGI("jam-ui", "no saved state, writing defaults"); save_state(); return; }
    size_t got = fread(&st_buf, 1, sizeof st_buf, f); fclose(f);
    if (got != sizeof st_buf || st_buf.magic != JAM_MAGIC) { ESP_LOGW("jam-ui", "state file from another version, replaced with defaults"); save_state(); return; }
    int max; float *buf = jam_sample_buf(&max);
    if (st_buf.smp_len > 0 && buf) {
        FILE *sf = fopen(SAMPLE_F, "rb");
        int len = 0;
        if (sf) {
            static int16_t chunk[1024];
            while (len < st_buf.smp_len && len < max) {
                int n = fread(chunk, sizeof(int16_t), 1024, sf); if (n <= 0) break;
                for (int k = 0; k < n && len < max; k++) buf[len++] = chunk[k] / 32768.0f;
            }
            fclose(sf);
        }
        st_buf.smp_len = len;
    } else st_buf.smp_len = 0;
    jam_set_state(&st_buf);
    ESP_LOGI("jam-ui", "loaded state: %d bpm, sample %d samples", (int)st_buf.bpm, (int)st_buf.smp_len);
}

// ---- drawing ------------------------------------------------------------------
static void header(const char *left, const char *right)
{
    oled_rect(0, 0, OLED_W, 9, true);
    oled_text_c(2, 1, left, false);
    if (right) oled_text_c(OLED_W - 2 - 6 * (int)strlen(right), 1, right, false);
}

static void dotted_vline(int x, int y0, int y1) { for (int y = y0; y <= y1; y += 2) oled_pixel(x, y, true); }

static void draw_pattern(const jam_ui_t *u)
{
    char l[32], r[48];
    int bar = cur_step / 16, rows = u->rows;
    const char *pf = !u->perform ? "" : u->sel == CH_SAMPLE ? (u->perform == PF_A ? " SCR" : " ROLL") : u->sel == CH_DRUMS ? " ROLL" : " PERF";
    snprintf(l, sizeof l, "%c %s", 'A' + u->pat, jam_ch_name(u->sel));
    snprintf(r, sizeof r, "%s%s%d %s", u->rec ? "REC " : "", pf[0] ? pf + 1 : "", u->bpm, u->scale);
    if (pf[0]) snprintf(r, sizeof r, "%s %d %s", pf + 1, u->bpm, u->scale);
    header(l, r);
    if (u->sel == CH_MIC) {
        int px = (int)((u->mic_db + 60) / 60 * 104); if (px < 0) px = 0; if (px > 104) px = 104;
        oled_text(12, 16, "LIVE MIC");
        oled_rect(12, 28, 104, 9, false); oled_rect(14, 30, px, 5, true);
        for (int d = 0; d <= 104; d += 26) oled_pixel(12 + d, 38, true);
        oled_text(12, 44, u->rec ? "recording, tap to stop" : "tap: record sample");
        if (u->sample_ok) { snprintf(l, sizeof l, "sample %.2fs", u->sample_s); oled_text(12, 54, l); }
        return;
    }
    const int gx = 12, cw = 7, top = 11, area = 43;
    int h = rows ? area / rows : 8; if (h > 14) h = 14;
    int gy = top + (area - h * rows) / 2;
    int col0 = bar * 16;
    // beat lines
    for (int k = 0; k <= 4; k++) dotted_vline(gx + k * 4 * cw - 1, gy, gy + h * rows - 1);
    for (int rI = 0; rI < rows; rI++) {
        int y = gy + rI * h;
        if (u->sel == CH_DRUMS) oled_text(2, y + (h - 7) / 2, u->row_label[rI]);
        else if (h >= 8) oled_text(u->root_row[rI] ? 1 : 3, y + (h - 7) / 2, u->row_label[rI]);
        else if (u->root_row[rI]) oled_rect(1, y + h / 2 - 1, 4, 2, true);
        for (int c = 0; c < 16; c++) {
            int x = gx + c * cw; uint8_t g = u->grid[rI][col0 + c];
            if (g == 1) oled_rect(x + 1, y + 1, cw - 2, h - 2, true);
            else if (g == 2) { oled_rect(x + 1, y + 1, cw - 2, h - 2, false); oled_pixel(x + cw / 2, y + h / 2, true); }
        }
    }
    // cursor: frame, blinking
    if ((frame / 6) & 1) { int cx = gx + (cur_step % 16) * cw, cy = gy + (rows - 1 - cur_row) * h; oled_rect(cx, cy, cw, h, false); }
    // step lights
    int ly = 57;
    for (int c = 0; c < 16; c++) {
        int x = gx + c * cw + 1;
        bool now = u->step / 16 == bar && u->step % 16 == c;
        if (now) oled_rect(x, ly, cw - 2, 5, true);
        else if (c % 4 == 0) oled_rect(x, ly, cw - 2, 5, false);
        else oled_pixel(x + 2, ly + 2, true);
    }
    snprintf(l, sizeof l, "%d", bar + 1);
    if (u->step / 16 == bar) { oled_rect(1, ly - 1, 8, 8, true); oled_text_c(2, ly, l, false); } else oled_text(2, ly, l);
}

static void draw_mix(const jam_ui_t *u)
{
    static const char *F[MX_N] = { "vol", "pan", "rev", "dly", "fx", "amt" };
    static const char *SH[CH_N] = { "DR", "BA", "LD", "PD", "SM", "MC" };
    static const char FXC[FX_N] = { '-', 'D', 'C', 'H', 'P', 'W', 'T' };
    char r[24]; float v = u->mix[ch][mix_field];
    if (mix_field == MX_FX) snprintf(r, sizeof r, "fx %s", mix_fx_name((int)v));
    else if (mix_field == MX_PAN) snprintf(r, sizeof r, "pan %+d", (int)(v * 100));
    else snprintf(r, sizeof r, "%s %d%%", F[mix_field], (int)(v * 100 + 0.5f));
    header("MIX", r);
    for (int i = 0; i < CH_N; i++) {
        int x = 1 + i * 21; bool sel = i == ch;
        if (sel) { oled_rect(x, 11, 20, 9, true); oled_text_c(x + 1, 12, SH[i], false); }
        else oled_text(x + 1, 12, SH[i]);
        if (u->mute[i]) oled_line(x + 2, 19, x + 16, 11, !sel);
        // fader
        int fx0 = x + 8, fy0 = 22, fh = 26;
        oled_rect(fx0, fy0, 3, fh, false);
        int k = (int)(u->mix[i][MX_VOL] * (fh - 3));
        oled_rect(fx0 - 3, fy0 + fh - 3 - k, 9, 3, !u->mute[i]);
        // pan
        int py = 51; oled_hline(x + 2, x + 16, py, true);
        int px = x + 9 + (int)(u->mix[i][MX_PAN] * 7); oled_rect(px - 1, py - 1, 3, 3, true);
        // sends (rev | dly) on one row, then fx letter and amount
        oled_rect(x + 2, 54, 1 + (int)(u->mix[i][MX_REV] * 7), 3, true);
        oled_rect(x + 11, 54, 1 + (int)(u->mix[i][MX_DLY] * 7), 3, true);
        char c[2] = { FXC[((int)u->mix[i][MX_FX]) % FX_N], 0 }; oled_text_c(x + 14, 12, c, !sel);
        oled_rect(x + 2, 59, 1 + (int)(u->mix[i][MX_AMT] * 16), 3, true);
        if (sel) {   // marker under the selected field
            int mx = x + 2, my = 63, mw = 16;
            if (mix_field == MX_VOL) { mx = fx0 - 4; my = fy0 + fh + 1; mw = 11; }
            else if (mix_field == MX_PAN) { my = py + 3; }
            else if (mix_field == MX_REV) { mw = 8; my = 58; }
            else if (mix_field == MX_DLY) { mx = x + 11; mw = 8; my = 58; }
            else if (mix_field == MX_FX) { mx = x + 13; my = 20; mw = 7; }
            else { mw = 17; }
            oled_hline(mx, mx + mw - 1, my, true);
        }
    }
}

static void draw_song(const jam_ui_t *u)
{
    char r[48], t[8];
    if (u->song_on) snprintf(r, sizeof r, "PLAY %d x%d/%d", u->song_pos + 1, u->song_rep + 1, u->reps[u->song_pos]);
    else snprintf(r, sizeof r, "STOP  %c playing", 'A' + u->pat);
    header("SONG", r);
    for (int i = 0; i < JAM_SONG; i++) {
        int x = 1 + (i % 8) * 16, y = 12 + (i / 8) * 26;
        bool playing = u->song_on && i == u->song_pos;
        int8_t bk = u->song[i];
        if (playing) oled_rect(x, y, 15, 24, true);
        t[0] = bk < 0 ? '-' : 'A' + bk; t[1] = 0;
        if (playing) { oled_rect(x + 2, y + 2, 11, 12, false); oled_text_c(x + 5, y + 5, t, false); }
        else oled_text2(x + 2, y + 1, t);
        if (bk >= 0) { snprintf(t, sizeof t, "x%d", u->reps[i]); oled_text_c(x + 2, y + 16, t, !playing); }
        if (i == song_slot && ((frame / 6) & 1)) oled_rect(x - 1, y - 1, 17, 26, false);
        if (bk < 0) break;   // nothing after the end marker
    }
    oled_text(1, 57, "turn:bank  push:reps");
}

static void draw_setup(const jam_ui_t *u)
{
    char r[24];
    snprintf(r, sizeof r, "%c %d", 'A' + u->pat, u->bpm);
    header("SETUP", r);
    int first = setup_field > 5 ? setup_field - 5 : 0;
    for (int k = 0; k < 6 && first + k < P_N; k++) {
        int p = first + k, y = 11 + k * 9; bool sel = p == setup_field;
        if (sel) oled_rect(0, y - 1, 124, 9, true);
        oled_text_c(2, y, jam_param_name(p), !sel);
        oled_text_c(122 - 6 * (int)strlen(u->param[p]), y, u->param[p], !sel);
    }
    // scrollbar
    oled_rect(126, 10, 2, 54, false);
    int sh = 54 * 6 / P_N, sy = 10 + (54 - sh) * first / (P_N - 6);
    oled_rect(126, sy, 2, sh, true);
}

void app_jam_run(i2c_master_bus_handle_t bus)
{
    (void)bus;
    input_init();
    mounted = storage_mount();
    load_all();
    uint32_t seen_dirty = jam_dirty(), seen_gen = jam_sample_gen(); int64_t dirty_at = 0;
    ESP_LOGI(TAG, "JAM: encoder = cursor/value, pushed+turn = channel, tap = toggle, hold = page, 3 s = clear channel; joystick click = perform");
    bool turned = false; int64_t last_log = 0;
    jam_ui_t u; jam_get_ui(&u);
    while (1) {
        input_ev_t in; input_poll(&in);
        frame++;
        int rows = u.rows;
        // --- navigation & values ---
        if (in.enc) {
            turned = turned || in.pressed;
            if (in.pressed) {
                if (page == PG_SETUP) setup_field = (setup_field + in.enc + P_N) % P_N;
                else if (page == PG_SONG) jam_cmd(CMD_SONG_REPS, song_slot, in.enc, 0);
                else { ch = (ch + in.enc + CH_N) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); cur_row = 0; if (perform) { perform = 0; jam_cmd(CMD_PERFORM, ch, 0, 0); } }
            }
            else if (page == PG_PATTERN) { if (ch != CH_MIC) cur_step = (cur_step + in.enc + JAM_STEPS) % JAM_STEPS; }
            else if (page == PG_MIX) jam_cmd(CMD_MIX, ch, mix_field, in.enc);
            else if (page == PG_SONG) jam_cmd(CMD_SONG_BANK, song_slot, in.enc, 0);
            else if (setup_field != P_CLEAR && setup_field != P_COPY) jam_cmd(CMD_PARAM, 0, setup_field, in.enc);
        }
        if (in.has_joy && in.pressed && (in.flick_u || in.flick_d) && page == PG_PATTERN) {   // encoder held + flick up/down: bank
            jam_cmd(CMD_PATTERN, 0, in.flick_u ? 1 : -1, 0); turned = true;
        } else if (in.has_joy && !perform) {
            if (page == PG_PATTERN) {
                if (in.flick_r) cur_step = (cur_step + 1) % JAM_STEPS;
                if (in.flick_l) cur_step = (cur_step + JAM_STEPS - 1) % JAM_STEPS;
                if (in.flick_u && rows) cur_row = (cur_row + 1) % rows;
                if (in.flick_d && rows) cur_row = (cur_row + rows - 1) % rows;
            } else if (page == PG_MIX) {
                if (in.flick_r) { ch = (ch + 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); }
                if (in.flick_l) { ch = (ch + CH_N - 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); }
                if (in.flick_d) mix_field = (mix_field + 1) % MX_N;
                if (in.flick_u) mix_field = (mix_field + MX_N - 1) % MX_N;
            } else if (page == PG_SONG) {
                if (in.flick_r) song_slot = (song_slot + 1) % JAM_SONG;
                if (in.flick_l) song_slot = (song_slot + JAM_SONG - 1) % JAM_SONG;
            } else {
                if (in.flick_d) setup_field = (setup_field + 1) % P_N;
                if (in.flick_u) setup_field = (setup_field + P_N - 1) % P_N;
            }
        }
        if (in.has_joy && in.joy_click && page == PG_PATTERN) { perform = (perform + 1) % (ch == CH_SAMPLE ? 3 : 2); jam_cmd(CMD_PERFORM, ch, perform, 0); ESP_LOGI(TAG, "perform %d", perform); }
        if (perform) jam_expr(in.jx, in.jy);
        // --- encoder button ---
        if (in.longp) { if (!turned) { jam_cmd(CMD_CLEAR_CH, ch, 0, 0); ESP_LOGI(TAG, "clear %s", jam_ch_name(ch)); } }
        else if (in.hold) { if (!turned) { page = (page + 1) % PG_N; ESP_LOGI(TAG, "page %d", page); } }
        else if (in.tap && !turned) {
            if (page == PG_PATTERN) {
                if (ch == CH_MIC) jam_cmd(CMD_REC, ch, !u.rec, 0);
                else jam_cmd(CMD_TOGGLE, ch, cur_step, cur_row);
            } else if (page == PG_MIX) jam_cmd(CMD_MUTE, ch, 0, 0);
            else if (page == PG_SONG) { jam_cmd(CMD_SONG_PLAY, 0, !u.song_on, song_slot); ESP_LOGI(TAG, "song %s", u.song_on ? "stop" : "play"); }
            else if (setup_field == P_CLEAR) { jam_cmd(CMD_CLEAR_ALL, 0, 0, 0); ESP_LOGI(TAG, "clear all"); }
            else if (setup_field == P_COPY) { jam_cmd(CMD_COPY, 0, 0, 0); ESP_LOGI(TAG, "copy pattern"); }
        }
        if (!in.pressed) turned = false;
        if (rows && cur_row >= rows) cur_row = rows - 1;

        jam_get_ui(&u);
        if (oled_present()) {
            oled_clear();
            if (page == PG_PATTERN) draw_pattern(&u); else if (page == PG_MIX) draw_mix(&u); else if (page == PG_SONG) draw_song(&u); else draw_setup(&u);
            oled_flush();
        }
        int64_t now = esp_timer_get_time();
        if (jam_dirty() != seen_dirty) { seen_dirty = jam_dirty(); dirty_at = now; }
        if (dirty_at && now - dirty_at > 2000000) { dirty_at = 0; save_state(); }
        if (jam_sample_gen() != seen_gen) { seen_gen = jam_sample_gen(); save_sample(); save_state(); }
        if (now - last_log > 3000000) {
            last_log = now;
            const float blk = 64.0f * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1e6f / CONFIG_KIT_SAMPLE_RATE;
            ESP_LOGI(TAG, "page %d %s | %c %d bpm %s | step %2d cursor %2d/%d | song %s %d | cpu %2.0f%% (mix %.0f total %.0f)", page, jam_ch_name(ch), 'A' + u.pat, u.bpm, u.scale,
                     u.step, cur_step, cur_row, u.song_on ? "on" : "off", u.song_pos, audio_cpu_load() * 100, jam_prof[6] / blk * 100, jam_prof[7] / blk * 100);
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
