// Looper UI. One encoder, one joystick, tilt, a hand wave, and the same
// grammar on every page:
//   turn        page (or move in the menu, or change a value)
//   push        record a take (menu: select)
//   hold 0.6 s  menu (menu: back)
//   hold 3 s    clear the song
//   joystick    tilt = expression, click = mute layer, flick up = undo,
//               flick down = next take replaces, flick left/right = page
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

// ---- menu
enum { MI_ACTION, MI_TOGGLE, MI_PARAM };
typedef struct { const char *label; int kind; int arg; } menu_item_t;
enum { MA_UNDO, MA_CLEAR_LAYER, MA_MUTE, MA_REPLACE, MA_EXPORT, MA_USB, MA_CLEAR_SONG };
static menu_item_t menu[20];
static int menu_n, menu_cur, menu_top;
static bool menu_open, editing;

static void build_menu(const looper_ui_t *u)
{
    menu_n = 0;
    menu[menu_n++] = (menu_item_t){ "Undo take", MI_ACTION, MA_UNDO };
    menu[menu_n++] = (menu_item_t){ "Mute layer", MI_TOGGLE, MA_MUTE };
    menu[menu_n++] = (menu_item_t){ "Replace next", MI_TOGGLE, MA_REPLACE };
    menu[menu_n++] = (menu_item_t){ "Clear layer", MI_ACTION, MA_CLEAR_LAYER };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_VOL };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_REV };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_DLY };
    if (u->page == PG_DRUMS) {
        menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_KIT };
        menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_SWING };
        menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_HUMAN };
    }
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_PUMP };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_DRIVE };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_BPM };
    menu[menu_n++] = (menu_item_t){ NULL, MI_PARAM, PR_BARS };
    menu[menu_n++] = (menu_item_t){ "Export song", MI_ACTION, MA_EXPORT };
    menu[menu_n++] = (menu_item_t){ "USB drive", MI_ACTION, MA_USB };
    menu[menu_n++] = (menu_item_t){ "Clear song", MI_ACTION, MA_CLEAR_SONG };
    if (menu_cur >= menu_n) menu_cur = menu_n - 1;
}

static void menu_select(const looper_ui_t *u)
{
    menu_item_t *m = &menu[menu_cur];
    switch (m->kind) {
    case MI_PARAM: editing = !editing; break;
    case MI_TOGGLE:
        if (m->arg == MA_MUTE) looper_action(ACT_MUTE);
        else looper_action(ACT_REPLACE);
        break;
    default:
        switch (m->arg) {
        case MA_UNDO:        looper_action(ACT_UNDO); menu_open = false; break;
        case MA_CLEAR_LAYER: looper_action(ACT_CLEAR_LAYER); menu_open = false; break;
        case MA_CLEAR_SONG:  looper_action(ACT_CLEAR_SONG); menu_open = false; break;
        case MA_EXPORT:      song_export_start(); menu_open = false; break;
        case MA_USB:         usbmode_request(true); song_save_state(); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); break;
        }
    }
}

// ---- drawing
static uint8_t vocal_env[OLED_W];
static int64_t vocal_env_t;

static void update_vocal_env(void)
{
    int n = looper_loop_len();
    const uint8_t *b = looper_vocal_buf();
    if (!b || n <= 0) return;
    int per = n / OLED_W;
    for (int x = 0; x < OLED_W; x++) {
        int mx = 0;
        for (int i = x * per; i < (x + 1) * per; i += 7) {
            uint8_t u = ~b[i];
            int m = ((u & 0x70) >> 4) * 16 + (u & 0x0F);   // exponent-weighted magnitude, 0..127
            if (m > mx) mx = m;
        }
        vocal_env[x] = mx;
    }
}

static void draw_header(const looper_ui_t *u, bool blink, const input_ev_t *in)
{
    oled_text2(0, 0, looper_page_name(u->page));
    char st[8] = "";
    if (in->pressed && in->held_ms > 600) snprintf(st, sizeof st, in->held_ms > 3000 ? "CLEAR" : (in->held_ms > 2000 ? "3s..." : "MENU"));
    else if (u->armed) snprintf(st, sizeof st, "IN %d", u->beats_to_go);
    else if (u->rec) snprintf(st, sizeof st, blink ? "REC" : "");
    else if (u->bounce) snprintf(st, sizeof st, "EXPORT");
    if (st[0]) oled_text(OLED_W - 6 * strlen(st), 7, st);
    for (int b = 0; b < 4; b++) oled_circle(92 + b * 10, 3, 2, b == u->beat % 4);
    oled_hline(0, OLED_W - 1, 15, true);
}

static void draw_layers(const looper_ui_t *u, bool blink)
{
    static const char letters[PG_COUNT] = { 'D', 'B', 'C', 'L', 'V' };
    for (int p = 0; p < PG_COUNT; p++) {
        int x = 2 + p * 13, y = 17;
        bool fill = u->has[p];
        if (p == u->page && u->rec) fill = blink;
        if (fill) { oled_rect(x, y, 11, 11, true); char s[2] = { letters[p], 0 }; oled_text_c(x + 3, y + 2, s, false); }
        else { oled_rect(x, y, 11, 11, false); char s[2] = { letters[p], 0 }; oled_text(x + 3, y + 2, s); }
        if (u->mute[p]) { oled_line(x, y, x + 10, y + 10, !fill); oled_line(x + 10, y, x, y + 10, !fill); }
        if (p == u->page) oled_hline(x, x + 10, y + 12, true);
    }
    // right of the chips: key and tempo
    char line[20];
    snprintf(line, sizeof line, "%3.0f", u->bpm);
    oled_text(70, 18, line);
    oled_text(70, 26 - 8, "");
    oled_text(92, 18, u->key);
    snprintf(line, sizeof line, "%db", u->bars);
    oled_text(70 + 24, 18 + 0, "");
    oled_text(70, 26, line);
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
    const uint8_t *seq = looper_seq(u->page);
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
    // live note marker on the left
    if (u->live_note >= 0) {
        int y = y0 + h - 1 - (u->live_note - lo) * (h - 2) / (hi - lo);
        oled_rect(0, y - 1, 5, 3, true);
    }
    for (int b = 0; b <= u->bars; b++) oled_vline(8 + b * (OLED_W - 8) / (u->bars > 0 ? u->bars : 1) - (b ? 1 : 0), y0 + h - 2, y0 + h - 1, true);
}

static void draw_vocal(const looper_ui_t *u, int y0, int h)
{
    int64_t now = esp_timer_get_time();
    if (u->has[PG_VOCAL] && now - vocal_env_t > 300000) { update_vocal_env(); vocal_env_t = now; }
    int cy = y0 + h / 2;
    if (u->has[PG_VOCAL] || u->rec) {
        for (int x = 0; x < OLED_W; x++) {
            int a = vocal_env[x] * (h / 2 - 1) / 127;
            if (a) oled_vline(x, cy - a, cy + a, true);
        }
    } else oled_text(8, cy - 3, "sing: hard-tuned");
    // live waveform overlay
    int8_t wave[AUDIO_WAVE_N];
    audio_get_wave(wave);
    int prev = cy - wave[0] * (h / 2 - 1) / 128;
    for (int x = 1; x < OLED_W; x += 2) {
        int y = cy - wave[x] * (h / 2 - 1) / 128;
        oled_pixel(x, y, true);
        prev = y;
    }
    (void)prev;
}

static void draw_footer(const looper_ui_t *u, const char *edit_text)
{
    char line[24], nm[5];
    if (edit_text) snprintf(line, sizeof line, "%s", edit_text);
    else if (song_export_progress() >= 0 && song_export_progress() < 100) snprintf(line, sizeof line, "%s %d%%", song_export_status(), song_export_progress());
    else if (u->msg[0]) snprintf(line, sizeof line, "%s", u->msg);
    else if (u->page == PG_DRUMS) {
        if (u->last_drum >= 0) snprintf(line, sizeof line, "%s %.2f/%.2f", drums_name(u->last_drum), u->drum_lo, u->drum_hi);
        else snprintf(line, sizeof line, "%s", kit_name(looper_param_get(PR_KIT)));
    } else if (u->live_note >= 0) snprintf(line, sizeof line, "%s", note_name(u->live_note, nm));
    else snprintf(line, sizeof line, "%s", u->has[u->page] ? (u->mute[u->page] ? "muted" : "playing") : "push: take");
    oled_text(0, 57, line);
    // VU with limiter marker
    float db = u->level > 1e-4f ? 20 * log10f(u->level) : -60;
    int w = (int)((db + 48) / 48 * 34);
    if (w < 0) w = 0;
    if (w > 34) w = 34;
    oled_rect(92, 58, 36, 5, false);
    oled_rect(93, 59, w, 3, true);
    if (u->gr > 0.02f) oled_rect(127 - (int)(u->gr * 30), 57, 2, 7, true);
}

static void draw_menu(void)
{
    const int x = 14, y = 12, w = 100, h = 42, rows = 4;
    oled_rect(x - 1, y - 1, w + 2, h + 2, true);
    oled_rect(x, y, w, h, false);
    oled_rect(x + 1, y + 1, w - 2, h - 2, true);
    oled_rect(x + 2, y + 2, w - 4, h - 4, false);
    if (menu_cur < menu_top) menu_top = menu_cur;
    if (menu_cur >= menu_top + rows) menu_top = menu_cur - rows + 1;
    for (int r = 0; r < rows && menu_top + r < menu_n; r++) {
        menu_item_t *m = &menu[menu_top + r];
        int yy = y + 4 + r * 9;
        bool sel = menu_top + r == menu_cur;
        char label[24], val[12] = "";
        if (m->kind == MI_PARAM) { snprintf(label, sizeof label, "%s", looper_param_name(m->arg)); looper_param_text(m->arg, val, sizeof val); }
        else {
            snprintf(label, sizeof label, "%s", m->label);
            if (m->kind == MI_TOGGLE) { looper_ui_t u; looper_get_ui(&u); snprintf(val, sizeof val, "%s", (m->arg == MA_MUTE ? u.mute[u.page] : u.replace) ? "on" : "off"); }
        }
        if (sel) oled_rect(x + 3, yy - 1, w - 6, 9, true);
        oled_text_c(x + 5, yy, label, !sel);
        if (val[0]) oled_text_c(x + w - 5 - 6 * strlen(val), yy, val, !sel);
        if (sel && editing) { oled_text_c(x + w - 10 - 6 * strlen(val), yy, "<", !sel); oled_text_c(x + w - 4, yy, ">", !sel); }
    }
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

// ---- main loop
void app_looper_run(i2c_master_bus_handle_t bus)
{
    input_init();
    imu_init(bus);
    tof_init(bus);
    draw_splash();
    if (oled_present()) vTaskDelay(pdMS_TO_TICKS(700));

    bool usb = usbmode_requested() || input_encoder_held_now();
    if (usb) {
        usbmode_request(false);
        usbmode_start(song_wl());
    } else {
        // the engine initialises itself in the audio task; give it a moment
        vTaskDelay(pdMS_TO_TICKS(50));
        song_load();
    }

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
            if (in.hold || in.longp) { esp_restart(); }
            if (oled_present() && now - last_draw > 200000) { draw_usb(); last_draw = now; }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // expression: joystick, tilt, hand
        float roll = 0, pitch = 0;
        imu_poll(&roll, &pitch);
        if (now - last_tof > 33000) { int mm = tof_poll(); if (mm > 0) tof_mm = mm; else if (tof_mm > 0 && now - last_tof > 300000) tof_mm = -1; last_tof = now; }
        float target = tof_mm > 0 && tof_mm < 400 ? clampf(1.0f - (tof_mm - 40) / 320.0f, 0, 1) : 0;
        space += 0.15f * (target - space);
        float cut = powf(2.0f, 1.5f * in.jx + clampf(pitch, -45, 45) / 45.0f);
        float vib = clampf(fabsf(roll) / 35.0f, 0, 1);
        looper_expr(cut, 2.0f * in.jy, imu_present() ? vib : 0, space);

        // controls
        if (menu_open) {
            build_menu(&u);
            if (in.enc) {
                if (editing) looper_param_step(menu[menu_cur].arg, in.enc > 0 ? 1 : -1);
                else { menu_cur += in.enc; if (menu_cur < 0) menu_cur = 0; if (menu_cur >= menu_n) menu_cur = menu_n - 1; }
            }
            if (in.tap) menu_select(&u);
            if (in.hold) { if (editing) editing = false; else menu_open = false; }
        } else {
            if (in.enc > 0) for (int i = 0; i < in.enc; i++) looper_action(ACT_NEXT_PAGE);
            if (in.enc < 0) for (int i = 0; i < -in.enc; i++) looper_action(ACT_PREV_PAGE);
            if (in.tap) looper_action(ACT_TAP);
            if (in.hold) { menu_open = true; editing = false; }
            if (in.flick_l) looper_action(ACT_PREV_PAGE);
            if (in.flick_r) looper_action(ACT_NEXT_PAGE);
            if (in.flick_u) looper_action(ACT_UNDO);
            if (in.flick_d) looper_action(ACT_REPLACE);
        }
        if (in.longp) { menu_open = false; looper_action(ACT_CLEAR_SONG); }
        if (in.joy_click) looper_action(ACT_MUTE);

        // autosave
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
            draw_layers(&u, blink);
            const int y0 = 31, h = 24;
            if (u.page == PG_DRUMS) draw_drums(&u, y0, h);
            else if (u.page == PG_VOCAL) draw_vocal(&u, y0, h);
            else draw_seq(&u, y0, h);
            if (u.steps > 0) { int x = 8 + u.step * (OLED_W - 8) / u.steps; oled_vline(x, y0 - 2, y0 - 1, true); }
            char edit[24]; const char *et = NULL;
            if (menu_open && editing) { char v[12]; looper_param_text(menu[menu_cur].arg, v, sizeof v); snprintf(edit, sizeof edit, "%s %s", looper_param_name(menu[menu_cur].arg), v); et = edit; }
            draw_footer(&u, et);
            if (menu_open) draw_menu();
            oled_flush();
            last_draw = now;
        }
        if (now - last_log > 1000000) {
            last_log = now;
            voice_t v; char nm[5];
            audio_get_voice(&v);
            extern uint32_t *looper_prof(void);
            uint32_t *pf = looper_prof();
            ESP_LOGI(TAG, "%-6s %s %-6s | %s %5.1f dB floor %5.1f %s conf %.2f | cpu %2.0f%% (in %u ren %u mix %u k/s) | joy %d imu %d tof %d tilt %+.0f/%+.0f | %s",
                     looper_page_name(u.page), u.rec ? "REC " : (u.armed ? "arm " : "play"), u.key,
                     v.gate ? "VOICE" : "quiet", v.db, voice_noise_db(), v.voiced ? note_name(v.note, nm) : "---", v.conf,
                     audio_cpu_load() * 100, (unsigned)(pf[0] >> 6), (unsigned)(pf[1] >> 6), (unsigned)(pf[2] >> 6), in.has_joy, imu_present(), tof_mm, roll, pitch, u.msg);
            pf[0] = pf[1] = pf[2] = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
