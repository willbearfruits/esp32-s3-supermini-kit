// Looper UI. One encoder, one joystick, tilt, a hand wave, and the same
// grammar everywhere:
//   turn        track (menu: move / change value)
//   push        record a take on this track (menu: select)
//   hold 0.6 s  menu (menu: back)
//   hold 3 s    clear all patterns
//   joystick    tilt = expression, click = mute track, flick up/down = scene,
//               flick left/right = track
//   body tilt   vibrato and brightness, hand over the sensor = space
#include "app.h"
#include "looper.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include "kit.h"
#include "input.h"
#include "imu.h"
#include "tof.h"
#include "song.h"
#include "usbmode.h"
#include "voice.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char *TAG = "ui";

// ---- two-level menu
enum { MI_ACTION, MI_TOGGLE, MI_PARAM, MI_SECTION };
typedef struct { const char *label; int kind; int arg; } item_t;
enum { SEC_TAKE, SEC_TRACK, SEC_SCENE, SEC_SONG, SEC_MASTER, SEC_FILES, SEC_N };
static const char *SEC_NAMES[SEC_N] = { "Take", "Track", "Scene", "Song", "Master", "Files" };
enum { MA_UNDO, MA_CLEAR_PAT, MA_REPLACE, MA_COPY_A, MA_MUTE, MA_SOLO, MA_DEL_TRACK, MA_ADD_TRACK,
       MA_SONG_MODE, MA_EXPORT, MA_SAVE, MA_LOAD, MA_NEW, MA_USB };
static item_t items[24];
static int n_items, cur[2], top;
static int level;                  // 0 sections, 1 items
static int section;
static bool menu_open, editing;
static int file_slot = 1;

static void build_items(const looper_ui_t *u)
{
    n_items = 0;
#define ADD(l, k, a) items[n_items++] = (item_t){ l, k, a }
    if (level == 0) { for (int i = 0; i < SEC_N; i++) ADD(SEC_NAMES[i], MI_SECTION, i); return; }
    switch (section) {
    case SEC_TAKE:
        ADD("Undo take", MI_ACTION, MA_UNDO); ADD("Replace next", MI_TOGGLE, MA_REPLACE);
        ADD("Copy A here", MI_ACTION, MA_COPY_A); ADD("Clear pattern", MI_ACTION, MA_CLEAR_PAT);
        break;
    case SEC_TRACK:
        ADD(NULL, MI_PARAM, PR_SOUND); ADD(NULL, MI_PARAM, PR_VOL); ADD(NULL, MI_PARAM, PR_PAN);
        ADD(NULL, MI_PARAM, PR_REV); ADD(NULL, MI_PARAM, PR_DLY); ADD(NULL, MI_PARAM, PR_LOWCUT); ADD(NULL, MI_PARAM, PR_TONE);
        ADD("Mute", MI_TOGGLE, MA_MUTE); ADD("Solo", MI_TOGGLE, MA_SOLO);
        ADD(NULL, MI_PARAM, PR_ADDKIND); ADD("Add track", MI_ACTION, MA_ADD_TRACK); ADD("Delete track", MI_ACTION, MA_DEL_TRACK);
        break;
    case SEC_SCENE:
        ADD(NULL, MI_PARAM, PR_SCENE); ADD("Song mode", MI_TOGGLE, MA_SONG_MODE); ADD(NULL, MI_PARAM, PR_ARR_REP);
        for (int i = 0; i < ARR_N; i++) ADD(NULL, MI_PARAM, PR_ARR0 + i);
        break;
    case SEC_SONG:
        ADD(NULL, MI_PARAM, PR_BPM); ADD(NULL, MI_PARAM, PR_BARS); ADD(NULL, MI_PARAM, PR_KEY);
        ADD(NULL, MI_PARAM, PR_COUNTIN); ADD(NULL, MI_PARAM, PR_METRO); ADD(NULL, MI_PARAM, PR_QUANT);
        ADD(NULL, MI_PARAM, PR_SWING); ADD(NULL, MI_PARAM, PR_HUMAN); ADD(NULL, MI_PARAM, PR_KIT);
        ADD(NULL, MI_PARAM, PR_MICGAIN); ADD(NULL, MI_PARAM, PR_GATE);
        break;
    case SEC_MASTER:
        ADD(NULL, MI_PARAM, PR_PUMP); ADD(NULL, MI_PARAM, PR_DRIVE); ADD(NULL, MI_PARAM, PR_MTONE);
        break;
    case SEC_FILES:
        ADD("Export scene", MI_ACTION, MA_EXPORT); ADD("Slot", MI_PARAM, -1);
        ADD("Save to slot", MI_ACTION, MA_SAVE); ADD("Load slot", MI_ACTION, MA_LOAD);
        ADD("New song", MI_ACTION, MA_NEW); ADD("USB drive", MI_ACTION, MA_USB);
        break;
    }
#undef ADD
    if (cur[level] >= n_items) cur[level] = n_items - 1;
    if (cur[level] < 0) cur[level] = 0;
}

static void item_step(item_t *m, int dir)
{
    if (m->kind != MI_PARAM) return;
    if (m->arg < 0) { file_slot += dir; if (file_slot < 1) file_slot = 1; if (file_slot > SONG_SLOTS) file_slot = SONG_SLOTS; return; }
    looper_param_step(m->arg, dir);
}

static void item_text(item_t *m, const looper_ui_t *u, char *label, int ll, char *val, int vl)
{
    val[0] = 0;
    if (m->kind == MI_PARAM) {
        if (m->arg < 0) { snprintf(label, ll, "Slot"); snprintf(val, vl, "%d%s", file_slot, file_slot == song_slot() ? "*" : ""); }
        else { snprintf(label, ll, "%s", looper_param_name(m->arg)); looper_param_text(m->arg, val, vl); }
        return;
    }
    snprintf(label, ll, "%s", m->label);
    if (m->kind == MI_TOGGLE) {
        bool on = m->arg == MA_MUTE ? u->tr[u->track].mute : (m->arg == MA_SOLO ? u->tr[u->track].solo : (m->arg == MA_REPLACE ? u->replace : u->song_mode));
        snprintf(val, vl, "%s", on ? "on" : "off");
    }
    if (m->kind == MI_SECTION) snprintf(val, vl, ">");
}

static void item_select(item_t *m)
{
    switch (m->kind) {
    case MI_SECTION: section = m->arg; level = 1; cur[1] = 0; break;
    case MI_PARAM:   editing = !editing; break;
    case MI_TOGGLE:
        if (m->arg == MA_MUTE) looper_action(ACT_MUTE);
        else if (m->arg == MA_SOLO) looper_action(ACT_SOLO);
        else if (m->arg == MA_REPLACE) looper_action(ACT_REPLACE);
        else looper_action(ACT_SONG_MODE);
        break;
    default:
        switch (m->arg) {
        case MA_UNDO:      looper_action(ACT_UNDO); menu_open = false; break;
        case MA_CLEAR_PAT: looper_action(ACT_CLEAR_PAT); menu_open = false; break;
        case MA_COPY_A:    looper_action(ACT_COPY_A); menu_open = false; break;
        case MA_DEL_TRACK: looper_action(ACT_DEL_TRACK); menu_open = false; break;
        case MA_ADD_TRACK: looper_action(ACT_ADD_TRACK); menu_open = false; break;
        case MA_EXPORT:    song_export_start(); menu_open = false; break;
        case MA_SAVE:      song_save_to(file_slot); menu_open = false; break;
        case MA_LOAD:      song_load(file_slot); menu_open = false; break;
        case MA_NEW:       looper_action(ACT_NEW_SONG); menu_open = false; break;
        case MA_USB:       usbmode_request(true); song_save_state(); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); break;
        }
    }
}

// ---- drawing
static uint8_t vocal_env[OLED_W];
static int64_t vocal_env_t;

static void update_vocal_env(const looper_ui_t *u)
{
    int slot = looper_vocal_slot(u->track, u->scene);
    if (slot < 0) slot = looper_vocal_slot(u->track, 0);
    const uint8_t *b = looper_vocal_data(slot);
    int n = looper_loop_len();
    if (!b || n <= 0 || slot < 0) { memset(vocal_env, 0, sizeof vocal_env); return; }
    int per = n / OLED_W;
    for (int x = 0; x < OLED_W; x++) {
        int mx = 0;
        for (int i = x * per; i < (x + 1) * per; i += 7) {
            uint8_t v = ~b[i];
            int m = ((v & 0x70) >> 4) * 16 + (v & 0x0F);
            if (m > mx) mx = m;
        }
        vocal_env[x] = mx;
    }
}

static void draw_header(const looper_ui_t *u, bool blink, const input_ev_t *in)
{
    oled_text2(0, 0, u->track_name);
    char st[8] = "";
    if (in->pressed && in->held_ms > 600) snprintf(st, sizeof st, in->held_ms > 3000 ? "CLEAR" : (in->held_ms > 2000 ? "3s..." : "MENU"));
    else if (u->armed) snprintf(st, sizeof st, "IN %d", u->beats_to_go);
    else if (u->rec) snprintf(st, sizeof st, blink ? "REC" : "");
    else if (u->bounce) snprintf(st, sizeof st, "EXPORT");
    if (st[0]) oled_text(OLED_W - 6 * strlen(st), 7, st);
    for (int b = 0; b < 4; b++) oled_circle(88 + b * 9, 3, 2, b == u->beat % 4);
    // scene box
    char sc[3] = { 'A' + u->scene, 0, 0 };
    if (u->scene_next >= 0 && u->scene_next != u->scene) { sc[0] = 'A' + u->scene_next; if (blink) sc[0] = ' '; }
    oled_rect(118, 0, 10, 9, true);
    oled_text_c(121, 1, sc, false);
    oled_hline(0, OLED_W - 1, 15, true);
}

static void draw_tracks(const looper_ui_t *u, bool blink)
{
    static const char letters[K_N] = { 'D', 'B', 'K', 'L', 'V' };
    for (int t = 0; t < u->n_tracks; t++) {
        int x = 1 + t * 12, y = 17;
        bool fill = u->tr[t].has;
        if (t == u->track && u->rec) fill = blink;
        char s[2] = { letters[u->tr[t].kind], 0 };
        if (fill) { oled_rect(x, y, 10, 10, true); oled_text_c(x + 3, y + 2, s, false); }
        else { oled_rect(x, y, 10, 10, false); oled_text(x + 3, y + 2, s); }
        if (u->tr[t].mute) { oled_line(x, y, x + 9, y + 9, !fill); oled_line(x + 9, y, x, y + 9, !fill); }
        if (u->tr[t].solo) oled_rect(x + 3, y - 2, 4, 1, true);
        if (t == u->track) oled_hline(x, x + 9, y + 11, true);
    }
    char line[16];
    snprintf(line, sizeof line, "%3.0f", u->bpm);
    oled_text(100, 17, line);
    oled_text(100, 25, u->key);
    if (u->song_mode) oled_text(100 - 12, 25, "S");
}

static void draw_drums(const looper_ui_t *u, int y0, int h)
{
    const uint8_t *pat = looper_drum_pattern();
    int steps = u->steps > 0 ? u->steps : 16;
    int rh = h / 3;
    static const char *lab[3] = { "H", "S", "K" };
    for (int row = 0; row < 3; row++) {
        int t = DRUM_N - 1 - row;
        int y = y0 + row * rh;
        oled_text(0, y + 1, lab[row]);
        for (int s = 0; s < steps; s++) {
            int x0 = 8 + s * (OLED_W - 8) / steps, x1 = 8 + (s + 1) * (OLED_W - 8) / steps;
            int vel = pat[s * DRUM_N + t];
            if (vel) {
                int hh = 2 + vel * (rh - 3) / 127;
                oled_rect(x0, y + rh - 1 - hh, x1 - x0 - 1 > 0 ? x1 - x0 - 1 : 1, hh, true);
            } else if (s % 4 == 0) oled_pixel(x0, y + rh - 2, true);
        }
    }
}

static void draw_seq(const looper_ui_t *u, int y0, int h)
{
    const uint8_t *seq = looper_seq();
    int steps = u->steps > 0 ? u->steps : 16;
    int lo = 127, hi = 0;
    for (int s = 0; s < steps; s++) {
        int n = seq[s] & 0x7F;
        if (!n) continue;
        if (n < lo) lo = n;
        if (n > hi) hi = n;
    }
    if (u->live_note >= 0) { if (u->live_note < lo) lo = u->live_note; if (u->live_note > hi) hi = u->live_note; }
    if (hi - lo < 12) { int c = (hi + lo) / 2; lo = c - 6; hi = c + 6; }
    for (int s = 0; s < steps; s++) {
        int n = seq[s] & 0x7F;
        if (!n) continue;
        int x0 = 8 + s * (OLED_W - 8) / steps, x1 = 8 + (s + 1) * (OLED_W - 8) / steps;
        int y = y0 + h - 1 - (n - lo) * (h - 2) / (hi - lo);
        if (seq[s] & SEQ_ATTACK) oled_rect(x0, y - 1, x1 - x0 > 1 ? x1 - x0 - 1 : 1, 3, true);
        else oled_hline(x0, x1 - 1, y, true);
    }
    if (u->live_note >= 0) {
        int y = y0 + h - 1 - (u->live_note - lo) * (h - 2) / (hi - lo);
        oled_rect(0, y - 1, 5, 3, true);
    }
    for (int b = 0; b <= u->bars; b++) oled_vline(8 + b * (OLED_W - 8) / (u->bars > 0 ? u->bars : 1) - (b ? 1 : 0), y0 + h - 2, y0 + h - 1, true);
}

static void draw_vocal(const looper_ui_t *u, int y0, int h)
{
    int64_t now = esp_timer_get_time();
    if (now - vocal_env_t > 300000) { update_vocal_env(u); vocal_env_t = now; }
    int cy = y0 + h / 2;
    bool any = false;
    for (int x = 0; x < OLED_W; x++) {
        int a = vocal_env[x] * (h / 2 - 1) / 127;
        if (a) { oled_vline(x, cy - a, cy + a, true); any = true; }
    }
    if (!any && !u->rec) oled_text(8, cy - 3, u->sound);
    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    for (int x = 1; x < OLED_W; x += 2) oled_pixel(x, cy - wave[x] * (h / 2 - 1) / 128, true);
}

static void draw_footer(const looper_ui_t *u, const char *edit_text)
{
    char line[48], nm[5];
    if (edit_text) snprintf(line, sizeof line, "%s", edit_text);
    else if (song_export_progress() >= 0 && song_export_progress() < 100) snprintf(line, sizeof line, "%s %d%%", song_export_status(), song_export_progress());
    else if (u->msg[0]) snprintf(line, sizeof line, "%s", u->msg);
    else if (u->kind == K_DRUMS) {
        if (u->last_drum >= 0) snprintf(line, sizeof line, "%s %.2f/%.2f", drums_name(u->last_drum), u->drum_lo, u->drum_hi);
        else snprintf(line, sizeof line, "%s", u->sound);
    } else if (u->live_note >= 0) snprintf(line, sizeof line, "%s %s", note_name(u->live_note, nm), u->sound);
    else snprintf(line, sizeof line, "%s", u->tr[u->track].has ? (u->tr[u->track].mute ? "muted" : u->sound) : "push: take");
    oled_text(0, 57, line);
    float db = u->level > 1e-4f ? 20 * log10f(u->level) : -60;
    int w = (int)((db + 48) / 48 * 34);
    if (w < 0) w = 0;
    if (w > 34) w = 34;
    oled_rect(92, 58, 36, 5, false);
    oled_rect(93, 59, w, 3, true);
    if (u->gr > 0.02f) oled_rect(127 - (int)(u->gr * 30), 57, 2, 7, true);
}

static void draw_menu(const looper_ui_t *u)
{
    const int x = 10, y = 12, w = 108, h = 44, rows = 4;
    oled_rect(x - 1, y - 1, w + 2, h + 2, true);
    oled_rect(x, y, w, h, false);
    oled_rect(x + 1, y + 1, w - 2, h - 2, true);
    oled_rect(x + 2, y + 2, w - 4, h - 4, false);
    int c = cur[level];
    if (c < top) top = c;
    if (c >= top + rows) top = c - rows + 1;
    if (top > n_items - rows) top = n_items - rows < 0 ? 0 : n_items - rows;
    for (int r = 0; r < rows && top + r < n_items; r++) {
        item_t *m = &items[top + r];
        int yy = y + 5 + r * 9;
        bool sel = top + r == c;
        char label[24], val[12];
        item_text(m, u, label, sizeof label, val, sizeof val);
        if (sel) oled_rect(x + 3, yy - 1, w - 6, 9, true);
        oled_text_c(x + 5, yy, label, !sel);
        if (val[0]) oled_text_c(x + w - 5 - 6 * strlen(val), yy, val, !sel);
        if (sel && editing) { oled_text_c(x + w - 11 - 6 * strlen(val), yy, "<", !sel); oled_text_c(x + w - 4, yy, ">", !sel); }
    }
    if (level == 1) oled_text(x + 4, y - 8, SEC_NAMES[section]);
}

static void draw_usb(void)
{
    oled_clear();
    oled_text2(10, 4, "USB DRIVE");
    oled_text(4, 24, usbmode_host_connected() ? "connected to the PC" : "waiting for the PC");
    oled_text(4, 34, "kits/user/*.wav in");
    oled_text(4, 44, "export/ out, MIDI on");
    oled_text(4, 56, "hold: back to looper");
    oled_flush();
}

static void draw_splash(void)
{
    oled_clear();
    oled_text2(16, 10, "KIT LOOPER");
    oled_text(22, 34, "voice to track");
    oled_rect(0, 62, OLED_W, 2, true);
    oled_flush();
}

void app_looper_run(i2c_master_bus_handle_t bus)
{
    input_init();
    imu_init(bus);
    tof_init(bus);
    draw_splash();
    if (oled_present()) vTaskDelay(pdMS_TO_TICKS(600));

    bool usb = usbmode_requested() || input_encoder_held_now();
    for (int i = 0; i < 100 && !looper_ready(); i++) vTaskDelay(pdMS_TO_TICKS(10));
    if (usb) { usbmode_request(false); usbmode_start(song_wl()); }
    else { song_load(song_slot()); file_slot = song_slot(); }

    int64_t last_draw = 0, last_log = 0, dirty_t = 0, last_tof = 0;
    int dirty = 0, tof_mm = -1;
    float space = 0;
    while (1) {
        int64_t now = esp_timer_get_time();
        input_ev_t in;
        input_poll(&in);
        looper_ui_t u;
        looper_get_ui(&u);

        if (usb) {
            if (in.hold || in.longp) esp_restart();
            if (oled_present() && now - last_draw > 200000) { draw_usb(); last_draw = now; }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float roll = 0, pitch = 0;
        imu_poll(&roll, &pitch);
        if (now - last_tof > 33000) { int mm = tof_poll(); if (mm > 0) tof_mm = mm; else if (tof_mm > 0 && now - last_tof > 300000) tof_mm = -1; last_tof = now; }
        float target = tof_mm > 0 && tof_mm < 400 ? clampf(1.0f - (tof_mm - 40) / 320.0f, 0, 1) : 0;
        space += 0.15f * (target - space);
        float cutm = powf(2.0f, 1.5f * in.jx + clampf(pitch, -45, 45) / 45.0f);
        float vib = clampf(fabsf(roll) / 35.0f, 0, 1);
        looper_expr(cutm, 2.0f * in.jy, imu_present() ? vib : 0, space);

        if (menu_open) {
            build_items(&u);
            if (in.enc) {
                if (editing) item_step(&items[cur[level]], in.enc > 0 ? 1 : -1);
                else { cur[level] += in.enc; if (cur[level] < 0) cur[level] = 0; if (cur[level] >= n_items) cur[level] = n_items - 1; }
            }
            if (in.tap) item_select(&items[cur[level]]);
            if (in.hold) { if (editing) editing = false; else if (level == 1) level = 0; else menu_open = false; }
        } else {
            if (in.enc > 0) for (int i = 0; i < in.enc; i++) looper_action(ACT_NEXT_TRACK);
            if (in.enc < 0) for (int i = 0; i < -in.enc; i++) looper_action(ACT_PREV_TRACK);
            if (in.tap) looper_action(ACT_TAP);
            if (in.hold) { menu_open = true; editing = false; level = 0; }
            if (in.flick_l) looper_action(ACT_PREV_TRACK);
            if (in.flick_r) looper_action(ACT_NEXT_TRACK);
            if (in.flick_u) looper_action(ACT_SCENE_UP);
            if (in.flick_d) looper_action(ACT_SCENE_DOWN);
        }
        if (in.longp) { menu_open = false; looper_action(ACT_CLEAR_SONG); }
        if (in.joy_click) looper_action(ACT_MUTE);

        int d = looper_take_dirty();
        if (d) { dirty |= d; dirty_t = now; }
        if (dirty && now - dirty_t > 800000) {
            if (dirty & DIRTY_STATE) song_save_state();
            if (dirty & DIRTY_VOCAL) song_save_vocal();
            dirty = 0;
        }

        if (oled_present() && now - last_draw > 40000) {
            bool blink = (now / 250000) & 1;
            oled_clear();
            draw_header(&u, blink, &in);
            draw_tracks(&u, blink);
            const int y0 = 31, h = 24;
            if (u.kind == K_DRUMS) draw_drums(&u, y0, h);
            else if (u.kind == K_VOCAL) draw_vocal(&u, y0, h);
            else draw_seq(&u, y0, h);
            if (u.steps > 0) { int x = 8 + u.step * (OLED_W - 8) / u.steps; oled_vline(x, y0 - 2, y0 - 1, true); }
            char edit[40]; const char *et = NULL;
            if (menu_open && editing) { char l[24], v[12]; item_text(&items[cur[level]], &u, l, sizeof l, v, sizeof v); snprintf(edit, sizeof edit, "%s %s", l, v); et = edit; }
            draw_footer(&u, et);
            if (menu_open) draw_menu(&u);
            oled_flush();
            last_draw = now;
        }
        if (now - last_log > 1000000) {
            last_log = now;
            voice_t v; char nm[5];
            audio_get_voice(&v);
            extern uint32_t *looper_prof(void);
            uint32_t *pf = looper_prof();
            ESP_LOGI(TAG, "%-8s %c %s %-6s | %s %5.1f dB floor %5.1f %s conf %.2f | cpu %2.0f%% (in %u ren %u mix %u k/s) | joy %d imu %d tof %d | %s",
                     u.track_name, 'A' + u.scene, u.rec ? "REC " : (u.armed ? "arm " : "play"), u.key,
                     v.gate ? "VOICE" : "quiet", v.db, voice_noise_db(), v.voiced ? note_name(v.note, nm) : "---", v.conf,
                     audio_cpu_load() * 100, (unsigned)(pf[0] >> 6), (unsigned)(pf[1] >> 6), (unsigned)(pf[2] >> 6),
                     in.has_joy, imu_present(), tof_mm, u.msg);
            pf[0] = pf[1] = pf[2] = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
