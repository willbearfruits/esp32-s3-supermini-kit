// JAM UI task: three pages, driven by the encoder and the joystick only
// (BOOT is inside the case).
//   PATTERN  step grid of the selected channel. Encoder turn moves the
//            cursor along the steps, joystick flicks move it (left/right
//            step, up/down row), encoder tap toggles the cell. On MIC the
//            tap starts and stops sample recording. Joystick click toggles
//            PERFORM: the stick then plays the channel (scratch on SAMPLE,
//            bend and filter on the synths, effect sends on PAD and MIC).
//   MIX      one row per channel: vol, pan, rev, dly. Flick left/right picks
//            the field, up/down the channel, encoder turn adjusts, tap mutes.
//   SETUP    bpm, root, scale, drum pattern, swing, kit, presets, clear all.
//            Flick up/down picks the field, encoder turn adjusts, tap fires
//            CLEAR ALL.
// Everywhere: encoder pushed + turned = next/previous channel (SETUP: field),
// encoder held 0.5 s = next page, held 3 s = clear the channel.
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
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <sys/stat.h>

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

static const char *TAG = "jam-ui";
extern float jam_prof[8];
enum { PG_PATTERN, PG_MIX, PG_SETUP, PG_N };
static int page, ch, cur_step, cur_row, mix_field, setup_field;
static int perform;   // PF_*

static void draw_pattern(const jam_ui_t *u)
{
    char line[64];
    int bar = cur_step / 16;
    const char *pf = !u->perform ? "" : u->sel == CH_SAMPLE ? (u->perform == PF_A ? " SCR" : " ROLL") : u->sel == CH_DRUMS ? " ROLL" : " PERF";
    snprintf(line, sizeof line, "%-6s b%d %3d %s%s%s", jam_ch_name(u->sel), bar + 1, u->bpm, u->scale, pf, u->rec ? " REC" : "");
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
            if (g == 1) oled_rect(x + 1, y + 1, 5, h - 2, true);
            else if (g == 2) { oled_rect(x + 1, y + 1, 5, h - 2, false); oled_pixel(x + 3, y + h / 2, true); }
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
    mounted = storage_mount();
    load_all();
    uint32_t seen_dirty = jam_dirty(), seen_gen = jam_sample_gen(); int64_t dirty_at = 0;
    ESP_LOGI(TAG, "JAM: encoder = cursor/value, pushed+turn = channel, tap = toggle, hold = page, 3 s = clear channel; joystick click = perform");
    bool turned = false; int64_t last_log = 0;
    jam_ui_t u; jam_get_ui(&u);
    while (1) {
        input_ev_t in; input_poll(&in);
        int rows = u.rows;
        // --- navigation & values ---
        if (in.enc) {
            turned = turned || in.pressed;
            if (in.pressed) {
                if (page == PG_SETUP) setup_field = (setup_field + in.enc + P_N) % P_N;
                else { ch = (ch + in.enc + CH_N) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); cur_row = 0; if (perform) { perform = 0; jam_cmd(CMD_PERFORM, ch, 0, 0); } }
            }
            else if (page == PG_PATTERN) { if (ch != CH_MIC) cur_step = (cur_step + in.enc + JAM_STEPS) % JAM_STEPS; }
            else if (page == PG_MIX) jam_cmd(CMD_MIX, ch, mix_field, in.enc);
            else if (setup_field < P_N - 1) jam_cmd(CMD_PARAM, 0, setup_field, in.enc);
        }
        if (in.has_joy && !perform) {
            if (in.flick_r) { if (page == PG_PATTERN) cur_step = (cur_step + 1) % JAM_STEPS; else if (page == PG_MIX) mix_field = (mix_field + 1) % MX_N; }
            if (in.flick_l) { if (page == PG_PATTERN) cur_step = (cur_step + JAM_STEPS - 1) % JAM_STEPS; else if (page == PG_MIX) mix_field = (mix_field + MX_N - 1) % MX_N; }
            if (in.flick_u) { if (page == PG_PATTERN && rows) cur_row = (cur_row + 1) % rows; else if (page == PG_SETUP) setup_field = (setup_field + P_N - 1) % P_N; else if (page == PG_MIX) { ch = (ch + CH_N - 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); } }
            if (in.flick_d) { if (page == PG_PATTERN && rows) cur_row = (cur_row + rows - 1) % rows; else if (page == PG_SETUP) setup_field = (setup_field + 1) % P_N; else if (page == PG_MIX) { ch = (ch + 1) % CH_N; jam_cmd(CMD_SELECT, ch, 0, 0); } }
        }
        if (in.has_joy && in.joy_click && page == PG_PATTERN) { perform = (perform + 1) % (ch == CH_SAMPLE ? 3 : 2); jam_cmd(CMD_PERFORM, ch, perform, 0); ESP_LOGI(TAG, "perform %d", perform); }
        if (perform) jam_expr(in.jx, in.jy);
        // --- encoder button ---
        if (in.longp) { if (!turned) { jam_cmd(CMD_CLEAR_CH, ch, 0, 0); ESP_LOGI(TAG, "clear %s", jam_ch_name(ch)); } }
        else if (in.hold) { if (!turned) { page = (page + 1) % PG_N; ESP_LOGI(TAG, "page %s", page == PG_PATTERN ? "PATTERN" : page == PG_MIX ? "MIX" : "SETUP"); } }
        else if (in.tap && !turned) {
            if (page == PG_PATTERN) {
                if (ch == CH_MIC) jam_cmd(CMD_REC, ch, !u.rec, 0);
                else jam_cmd(CMD_TOGGLE, ch, cur_step, cur_row);
            } else if (page == PG_MIX) jam_cmd(CMD_MUTE, ch, 0, 0);
            else if (setup_field == P_N - 1) { jam_cmd(CMD_CLEAR_ALL, 0, 0, 0); ESP_LOGI(TAG, "clear all"); }
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
        // autosave 2 s after the last edit, and the sample right after a recording
        if (jam_dirty() != seen_dirty) { seen_dirty = jam_dirty(); dirty_at = now; }
        if (dirty_at && now - dirty_at > 2000000) { dirty_at = 0; save_state(); }
        if (jam_sample_gen() != seen_gen) { seen_gen = jam_sample_gen(); save_sample(); save_state(); }
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
