#include "jam.h"
#include "dsp.h"
#include "kit.h"
#include "synth.h"
#include "mix.h"
#include "faust_voice.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

static const char *TAG = "jam";
#define STEPS      JAM_STEPS
#define SAMPLE_MAX ((int)(2.0f * FS))
#define CMDQ       32
#define TRACKS     6
#define SV_N       4
#define PAD_MAX_STEPS 8
#define SLICES     8
#define REV        SLICES      // note value >= REV: reversed slice

// ---- scales --------------------------------------------------------------
typedef struct { const char *name; int n; int8_t iv[7]; } scale_def_t;
static const scale_def_t SCALES[] = {
    { "MAJOR",    7, { 0, 2, 4, 5, 7, 9, 11 } }, { "MINOR",    7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "DORIAN",   7, { 0, 2, 3, 5, 7, 9, 10 } }, { "MIXO",     7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "HARM MIN", 7, { 0, 2, 3, 5, 7, 8, 11 } }, { "PENT MAJ", 5, { 0, 2, 4, 7, 9 } },
    { "PENT MIN", 5, { 0, 3, 5, 7, 10 } },       { "BLUES",    6, { 0, 3, 5, 6, 7, 10 } },
};
#define SCALE_N (int)(sizeof SCALES / sizeof SCALES[0])
static const char *NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// ---- drum patterns: 16 steps, x accent, o medium, - soft, . rest. Picking one
// in SETUP copies it into both bars of the editable grid; OFF clears it. ----
typedef struct { const char *name, *kick, *snare, *hat; } style_t;
static const style_t STYLES[] = {
    { "OFF",    "................", "................", "................" },
    { "HOUSE",  "x...x...x...x...", "....x.......x...", "-.o.-.o.-.o.-.o." },
    { "HIPHOP", "x.....x..x......", "....x.......x...", "o.-.o.-.o.-.o.-." },
    { "TECHNO", "x...x...x...x...", "....-.......-...", "..x...x...x...x." },
    { "BREAKS", "x..x..x...x.x...", "....x..o.....x..", "o.-.o.-.o.-.o.-." },
    { "FUNK",   "x..x....x.x.....", "....x..-....x..-", "o-o-o-o-o-o-o-o-" },
};
#define STYLE_N (int)(sizeof STYLES / sizeof STYLES[0])
static uint8_t tvel(char c) { return c == 'x' ? 110 : c == 'o' ? 80 : c == '-' ? 45 : 0; }

typedef struct { bool on; float pos, rate, amp, end; } svoice_t;   // end: stop when pos passes it (either direction)
typedef struct { int cmd, ch, a, b; } cmd_t;

static struct {
    int   bpm, style, swing, root, scale, kit, bpre, lpre, ppre;
    float step_len, pos; int loop_len, next_step;
    int   sel, perform, smode; bool mute[CH_N]; float mix[CH_N][MX_N];
    uint8_t drum[STEPS][DRUM_N];                // velocity, 0 = none
    int8_t  note[4][STEPS];                     // bass, lead, pad, sample: row index or -1
    int   pad_off, bass_off, lead_off;          // steps at which a pending note ends, -1 none
    drums_t drums; fvoice_t *bv, *lv; synth_t pad;
    float bv_hz, lv_hz;                         // last note frequencies, for the bend
    float *smp; int smp_len; float smp_root; bool rec; int rec_pos;
    svoice_t sv[SV_N]; int sv_rr;
    float scratch_pos, scratch_rate;
    // ratchet: what fired last, and the roll clock
    uint8_t last_drum[DRUM_N]; int last_smp; int roll_acc, roll_n;
    float ex, ey;                               // joystick, perform mode
    cmd_t q[CMDQ]; volatile int qw, qr;
    float mic_db;
    volatile uint32_t dirty, smp_gen; const jam_state_t *pending;
} S;

static float *lay[TRACKS];
float jam_prof[8];

const char *jam_ch_name(int ch) { static const char *N[CH_N] = { "DRUMS", "BASS", "LEAD", "PAD", "SAMPLE", "MIC" }; return ch >= 0 && ch < CH_N ? N[ch] : "?"; }
const char *jam_param_name(int p) { static const char *N[P_N] = { "BPM", "ROOT", "SCALE", "DRUMS", "SWING", "KIT", "BASS", "LEAD", "PAD", "SAMPLE", "CLEAR" }; return p >= 0 && p < P_N ? N[p] : "?"; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float mtof(float n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }
static const scale_def_t *sc(void) { return &SCALES[S.scale]; }
static int rows_of(int ch) { return ch == CH_DRUMS ? DRUM_N : ch == CH_PAD ? sc()->n : ch == CH_MIC ? 0 : sc()->n + 1; }
static int lane(int ch) { return ch - CH_BASS; }   // index into note[] for bass, lead, pad, sample

// row index -> semitones above the channel's root
static int row_semi(int r) { const scale_def_t *s = sc(); return s->iv[r % s->n] + 12 * (r / s->n); }
static int ch_base(int ch) { return ch == CH_BASS ? 36 : ch == CH_LEAD ? 60 : 48; }   // midi of row 0 before the root

static void set_tempo(int bpm)
{
    float frac = S.loop_len > 0 ? S.pos / S.loop_len : 0;
    S.bpm = bpm;
    S.step_len = FS * 60.0f / bpm / 4.0f;
    S.loop_len = (int)(S.step_len * STEPS);
    S.pos = frac * S.loop_len;
    S.next_step = (int)(S.pos / S.step_len) + 1;
    mix_set_tempo(bpm);
}

static float step_time(int s)
{
    float t = s * S.step_len;
    if (s & 1) t += S.swing * 0.01f * S.step_len / 3.0f;
    return t;
}

// ---- voices ----------------------------------------------------------------
static void sample_on(float rate, float vel)   // whole sample, pitched
{
    if (S.smp_len <= 0) return;
    svoice_t *v = &S.sv[S.sv_rr]; S.sv_rr = (S.sv_rr + 1) % SV_N;
    v->on = true; v->pos = 0; v->rate = rate; v->amp = vel; v->end = S.smp_len - 1.001f;
}

static void slice_on(int val, float rate, float vel)   // slice val, or reversed when val >= REV
{
    if (S.smp_len < SLICES * 64) return;
    bool rev = val >= REV; int k = val % SLICES;
    float a = (float)S.smp_len * k / SLICES, b = (float)S.smp_len * (k + 1) / SLICES - 1.001f;
    svoice_t *v = &S.sv[S.sv_rr]; S.sv_rr = (S.sv_rr + 1) % SV_N;
    v->on = true; v->amp = vel;
    if (rev) { v->pos = b; v->rate = -rate; v->end = a; } else { v->pos = a; v->rate = rate; v->end = b; }
}

// play the sample event stored in the grid (pitch row or slice), used by the step and the roll
static void sample_event(int val, float pitch_mul, float vel)
{
    if (S.smode) slice_on(val, pitch_mul, vel);
    else sample_on(powf(2.0f, row_semi(val) / 12.0f) * pitch_mul, vel);
}

static void sample_render(float *out, int n)
{
    if (S.perform == PF_A && S.sel == CH_SAMPLE && S.smp_len > 1) {
        // scratch: the joystick is the hand on the record
        float target = S.ex * 3.0f;
        for (int i = 0; i < n; i++) {
            S.scratch_rate += 0.002f * (target - S.scratch_rate);
            S.scratch_pos += S.scratch_rate;
            if (S.scratch_pos < 0) S.scratch_pos = 0;
            if (S.scratch_pos >= S.smp_len - 1) S.scratch_pos = S.smp_len - 1.001f;
            int p = (int)S.scratch_pos; float f = S.scratch_pos - p;
            out[i] += (S.smp[p] + (S.smp[p + 1] - S.smp[p]) * f) * 0.9f;
        }
        return;
    }
    for (int k = 0; k < SV_N; k++) {
        svoice_t *v = &S.sv[k];
        if (!v->on) continue;
        for (int i = 0; i < n; i++) {
            if (v->pos < 0 || v->pos >= S.smp_len - 1 || (v->rate > 0 ? v->pos > v->end : v->pos < v->end)) { v->on = false; break; }
            int p = (int)v->pos; float f = v->pos - p;
            out[i] += (S.smp[p] + (S.smp[p + 1] - S.smp[p]) * f) * v->amp;
            v->pos += v->rate;
        }
    }
}

static void pad_chord(int row, bool on)
{
    synth_note_off(&S.pad, -1);
    if (!on) return;
    const scale_def_t *s = sc();
    int base = ch_base(CH_PAD) + S.root;
    for (int k = 0; k < 3; k++) {
        int d = row + 2 * k, oct = d / s->n; d %= s->n;
        synth_note_on(&S.pad, base + s->iv[d] + 12 * oct, 0.6f);
    }
}

// ---- sequencer -------------------------------------------------------------
static float human(float vel) { S.drums.seed = S.drums.seed * 1664525u + 1013904223u; return vel * (0.94f + 0.12f * ((S.drums.seed >> 8) & 1023) / 1023.0f); }

static void load_style(int idx)
{
    const style_t *st = &STYLES[idx]; const char *tpl[DRUM_N] = { st->kick, st->snare, st->hat };
    for (int s = 0; s < STEPS; s++) for (int t = 0; t < DRUM_N; t++) S.drum[s][t] = tvel(tpl[t][s % 16]);
}

static void fire_step(int s)
{
    if (!S.mute[CH_DRUMS]) {
        bool any = false;
        for (int t = 0; t < DRUM_N; t++) if (S.drum[s][t]) any = true;
        for (int t = 0; t < DRUM_N; t++) {
            int v = S.drum[s][t];
            if (any) S.last_drum[t] = v;
            if (v) { drums_trigger(&S.drums, t, human(v / 127.0f)); if (t == DRUM_KICK) mix_kick(); }
        }
    }
    if (S.bass_off == s) { S.bass_off = -1; fvoice_note(S.bv, 0, 0, false); }
    if (S.lead_off == s) { S.lead_off = -1; fvoice_note(S.lv, 0, 0, false); }
    if (S.pad_off == s)  { S.pad_off = -1; pad_chord(0, false); }
    int r;
    if ((r = S.note[lane(CH_BASS)][s]) >= 0 && !S.mute[CH_BASS]) {
        S.bv_hz = mtof(ch_base(CH_BASS) + S.root + row_semi(r)); fvoice_note(S.bv, S.bv_hz, 0.9f, true);
        S.bass_off = (s + 1) % STEPS;
    }
    if ((r = S.note[lane(CH_LEAD)][s]) >= 0 && !S.mute[CH_LEAD]) {
        S.lv_hz = mtof(ch_base(CH_LEAD) + S.root + row_semi(r)); fvoice_note(S.lv, S.lv_hz, 0.85f, true);
        S.lead_off = (s + 1) % STEPS;
    }
    if ((r = S.note[lane(CH_PAD)][s]) >= 0 && !S.mute[CH_PAD]) { pad_chord(r, true); S.pad_off = (s + PAD_MAX_STEPS) % STEPS; }
    if ((r = S.note[lane(CH_SAMPLE)][s]) >= 0 && !S.mute[CH_SAMPLE]) {
        S.last_smp = r;
        if (!(S.perform == PF_A && S.sel == CH_SAMPLE)) sample_event(r, 1.0f, 0.9f);
    }
}

// ---- sampler recording -------------------------------------------------------
static void rec_feed(const float *in, int n)
{
    if (!S.rec) return;
    int room = SAMPLE_MAX - S.rec_pos; if (n > room) n = room;
    memcpy(S.smp + S.rec_pos, in, n * sizeof(float));
    S.rec_pos += n;
    if (S.rec_pos >= SAMPLE_MAX) { S.rec = false; ESP_LOGI(TAG, "sample buffer full"); }
}

static void rec_stop(void)
{
    S.rec = false;
    int len = S.rec_pos, start = 0;
    float pk = 0; for (int i = 0; i < len; i++) if (fabsf(S.smp[i]) > pk) pk = fabsf(S.smp[i]);
    if (len < (int)(0.1f * FS) || pk < 0.005f) { ESP_LOGI(TAG, "sample too short or silent, kept the old one"); return; }
    float thr = pk * 0.05f;
    while (start < len && fabsf(S.smp[start]) < thr) start++;
    start = start > 64 ? start - 64 : 0;
    len -= start;
    memmove(S.smp, S.smp + start, len * sizeof(float));
    float g = 0.8f / pk; int fade = (int)(0.005f * FS);
    for (int i = 0; i < len; i++) {
        float w = i < fade ? (float)i / fade : len - i < fade ? (float)(len - i) / fade : 1.0f;
        S.smp[i] *= g * w;
    }
    S.smp_len = len;
    for (int k = 0; k < SV_N; k++) S.sv[k].on = false;
    S.scratch_pos = 0;
    S.smp_gen++;
    ESP_LOGI(TAG, "sample: %.2f s", len / FS);
}

// ---- commands ----------------------------------------------------------------
void jam_cmd(int cmd, int ch, int a, int b) { int w = S.qw; S.q[w % CMDQ] = (cmd_t){ cmd, ch, a, b }; S.qw = w + 1; }
void jam_expr(float x, float y) { S.ex = clampf(x, -1, 1); S.ey = clampf(y, -1, 1); }

static void clear_ch(int ch)
{
    if (ch == CH_DRUMS) memset(S.drum, 0, sizeof S.drum);
    else if (ch >= CH_BASS && ch <= CH_SAMPLE) memset(S.note[lane(ch)], -1, STEPS);
    if (ch == CH_BASS) fvoice_note(S.bv, 0, 0, false);
    if (ch == CH_LEAD) fvoice_note(S.lv, 0, 0, false);
    if (ch == CH_PAD) pad_chord(0, false);
}

static void set_param(int p, int d)
{
    switch (p) {
    case P_BPM:   set_tempo(clampi(S.bpm + d, 40, 220)); break;
    case P_ROOT:  S.root = (S.root + d + 12) % 12; break;
    case P_SCALE: S.scale = (S.scale + d + SCALE_N) % SCALE_N; break;
    case P_STYLE: S.style = (S.style + d + STYLE_N) % STYLE_N; load_style(S.style); break;
    case P_SWING: S.swing = clampi(S.swing + d * 5, 0, 100); break;
    case P_KIT:   S.kit = (S.kit + d + KIT_SYNTH_N) % KIT_SYNTH_N; drums_set_kit(&S.drums, S.kit); break;
    case P_BASS:  S.bpre = (S.bpre + d + FV_PRESETS) % FV_PRESETS; fvoice_preset(S.bv, FV_BASS, S.bpre); break;
    case P_LEAD:  S.lpre = (S.lpre + d + FV_PRESETS) % FV_PRESETS; fvoice_preset(S.lv, FV_LEAD, S.lpre); break;
    case P_PAD:   S.ppre = (S.ppre + d + SYNTH_PRESETS) % SYNTH_PRESETS; synth_init(&S.pad, synth_preset(SK_KEYS, S.ppre), 3); break;
    case P_SMODE: S.smode = !S.smode; memset(S.note[lane(CH_SAMPLE)], -1, STEPS); break;   // rows change meaning: start clean
    }
}

static void apply_state(const jam_state_t *st)
{
    if (!st || st->magic != JAM_MAGIC) return;
    set_tempo(clampi(st->bpm, 40, 220));
    S.style = clampi(st->style, 0, STYLE_N - 1); S.swing = clampi(st->swing, 0, 100);
    S.root = clampi(st->root, 0, 11); S.scale = clampi(st->scale, 0, SCALE_N - 1);
    S.kit = clampi(st->kit, 0, KIT_SYNTH_N - 1); drums_set_kit(&S.drums, S.kit);
    S.bpre = clampi(st->bpre, 0, FV_PRESETS - 1); fvoice_preset(S.bv, FV_BASS, S.bpre);
    S.lpre = clampi(st->lpre, 0, FV_PRESETS - 1); fvoice_preset(S.lv, FV_LEAD, S.lpre);
    S.ppre = clampi(st->ppre, 0, SYNTH_PRESETS - 1); synth_init(&S.pad, synth_preset(SK_KEYS, S.ppre), 3);
    for (int i = 0; i < CH_N; i++) S.mute[i] = st->mute[i];
    memcpy(S.mix, st->mix, sizeof S.mix);
    memcpy(S.drum, st->drum, sizeof S.drum);
    memcpy(S.note, st->note, sizeof S.note);
    S.smp_len = clampi(st->smp_len, 0, SAMPLE_MAX);
    S.smode = st->version >= 2 ? (st->smode != 0) : 0;
    for (int k = 0; k < SV_N; k++) S.sv[k].on = false;
}

static void do_cmd(const cmd_t *c)
{
    if (c->cmd != CMD_SELECT && c->cmd != CMD_PERFORM && c->cmd != CMD_LOAD) S.dirty++;
    switch (c->cmd) {
    case CMD_SELECT: S.sel = clampi(c->ch, 0, CH_N - 1); break;
    case CMD_TOGGLE: {   // ch, step, row
        int s = clampi(c->a, 0, STEPS - 1), r = clampi(c->b, 0, JAM_ROWS - 1);
        if (c->ch == CH_DRUMS) { if (r < DRUM_N) S.drum[s][r] = S.drum[s][r] ? 0 : 110; }
        else if (c->ch == CH_SAMPLE && S.smode) { int8_t *n = &S.note[lane(c->ch)][s]; *n = (*n == r) ? (int8_t)(r + REV) : (*n == r + REV) ? -1 : (int8_t)r; }
        else if (c->ch >= CH_BASS && c->ch <= CH_SAMPLE) { int8_t *n = &S.note[lane(c->ch)][s]; *n = (*n == r) ? -1 : (int8_t)r; }
        break; }
    case CMD_CLEAR_CH:  clear_ch(c->ch); break;
    case CMD_CLEAR_ALL: for (int ch = 0; ch < CH_N; ch++) clear_ch(ch); break;
    case CMD_MUTE:      S.mute[c->ch] = !S.mute[c->ch]; break;
    case CMD_MIX: {      // ch, field, delta (in 1/20 steps)
        float *v = &S.mix[c->ch][c->a];
        float lo = c->a == MX_PAN ? -1 : 0;
        *v = clampf(*v + c->b * 0.05f, lo, 1); break; }
    case CMD_PARAM:     set_param(c->a, c->b); break;
    case CMD_REC:       if (c->a) { if (!S.rec) { S.rec = true; S.rec_pos = 0; } } else if (S.rec) rec_stop(); break;
    case CMD_PERFORM:   S.perform = c->a; S.scratch_rate = 0; S.roll_acc = 0; S.roll_n = 0; break;
    case CMD_LOAD:      apply_state(S.pending); S.pending = NULL; break;
    }
}

// ---- fx_t --------------------------------------------------------------------
static void init(void)
{
    static bool once;
    if (once) return;
    once = true;
    for (int i = 0; i < TRACKS; i++) lay[i] = heap_caps_calloc(256, sizeof(float), MALLOC_CAP_INTERNAL);
    S.smp = heap_caps_malloc(SAMPLE_MAX * sizeof(float), MALLOC_CAP_SPIRAM);
    voice_set_pitch(false);            // JAM only needs the mic level
    mix_init();
    mix_params()->pump = 0.3f; mix_params()->rev_mix = 0.5f; mix_params()->dly_fb = 0.4f; mix_params()->drive = 1.0f; mix_params()->oversample = false;
    drums_init(&S.drums, 0);
    S.bv = fvoice_new(FV_BASS, 0);
    S.lv = fvoice_new(FV_LEAD, 0);
    S.ppre = 1; synth_init(&S.pad, synth_preset(SK_KEYS, S.ppre), 3);
    memset(S.note, -1, sizeof S.note);
    S.bass_off = S.lead_off = S.pad_off = -1; S.last_smp = -1;
    S.style = 0; S.scale = 0; S.root = 0;
    static const float MIXDEF[CH_N][MX_N] = {
        { 0.9f, 0.0f, 0.1f, 0.0f }, { 0.85f, 0.0f, 0.0f, 0.0f }, { 0.75f, 0.15f, 0.3f, 0.25f },
        { 0.6f, -0.15f, 0.5f, 0.0f }, { 0.85f, 0.1f, 0.25f, 0.15f }, { 0.8f, 0.0f, 0.3f, 0.2f } };
    memcpy(S.mix, MIXDEF, sizeof S.mix);
    set_tempo(CONFIG_KIT_BPM);
    S.pos = 0; S.next_step = 0;
    ESP_LOGI(TAG, "jam: %d bpm, %d steps, sample buffer %.1f s, free internal %u", S.bpm, STEPS, SAMPLE_MAX / FS, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static inline void prof(int i, uint32_t *c) { uint32_t t = esp_cpu_get_cycle_count(); jam_prof[i] += 0.02f * ((float)(t - *c) - jam_prof[i]); *c = t; }

static void IRAM_ATTR process(const float *in, float *out, int n, const voice_t *v)
{
    while (S.qr != S.qw) { do_cmd(&S.q[S.qr % CMDQ]); S.qr++; }
    S.mic_db = v->db;
    uint32_t c = esp_cpu_get_cycle_count(), c_all = c;
    rec_feed(in, n);

    float end = S.pos + n;
    while (S.next_step < STEPS && step_time(S.next_step) < end) { fire_step(S.next_step); S.next_step++; }
    S.pos = end;
    if (S.pos >= S.loop_len) { S.pos -= S.loop_len; S.next_step = 0; while (step_time(S.next_step) < S.pos) { fire_step(S.next_step); S.next_step++; } }

    // perform mode: joystick on the selected channel
    bool roll = S.perform && ((S.sel == CH_DRUMS && S.perform == PF_A) || (S.sel == CH_SAMPLE && S.perform == PF_B));
    if (roll) {
        float ax = fabsf(S.ex);
        if (ax < 0.15f) { S.roll_acc = 0; S.roll_n = 0; }
        else {
            float div = ax < 0.4f ? 0.5f : ax < 0.65f ? 1.0f : ax < 0.85f ? 2.0f : 4.0f;   // 1/8 1/16 1/32 1/64
            int interval = (int)(S.step_len / div);
            S.roll_acc += n;
            while (S.roll_acc >= interval) {
                S.roll_acc -= interval; S.roll_n++;
                float ramp = S.ey * S.roll_n;
                if (S.sel == CH_DRUMS && !S.mute[CH_DRUMS]) {
                    float g = powf(1.15f, ramp); if (g > 1.4f) g = 1.4f; if (g < 0.15f) g = 0.15f;
                    for (int t = 0; t < DRUM_N; t++) if (S.last_drum[t]) { drums_trigger(&S.drums, t, clampf(S.last_drum[t] / 127.0f * g, 0.1f, 1.0f)); if (t == DRUM_KICK) mix_kick(); }
                } else if (S.sel == CH_SAMPLE && S.last_smp >= 0 && !S.mute[CH_SAMPLE]) {
                    float semi = clampf(ramp * 2.0f, -24, 24);
                    sample_event(S.last_smp, powf(2.0f, semi / 12.0f), 0.9f);
                }
            }
        }
    }
    float bright = 1, bend = 0;
    if (S.perform && (S.sel == CH_BASS || S.sel == CH_LEAD || S.sel == CH_PAD)) { bright = powf(4.0f, S.ex); bend = S.ey * 2.0f; }
    fvoice_bright(S.bv, S.sel == CH_BASS ? bright : 1); fvoice_bright(S.lv, S.sel == CH_LEAD ? bright : 1);
    if (S.sel == CH_BASS && S.perform) fvoice_note(S.bv, S.bv_hz * powf(2.0f, bend / 12.0f), 0.9f, S.bass_off >= 0);
    if (S.sel == CH_LEAD && S.perform) fvoice_note(S.lv, S.lv_hz * powf(2.0f, bend / 12.0f), 0.85f, S.lead_off >= 0);
    synth_set_expr(&S.pad, S.sel == CH_PAD ? bend : 0, S.sel == CH_PAD ? bright : 1, 0);
    prof(0, &c);

    for (int i = 0; i < TRACKS; i++) memset(lay[i], 0, n * sizeof(float));
    drums_render(&S.drums, lay[CH_DRUMS], n, 0.8f); prof(1, &c);
    fvoice_render(S.bv, lay[CH_BASS], n); prof(2, &c);
    fvoice_render(S.lv, lay[CH_LEAD], n); prof(3, &c);
    synth_render(&S.pad, lay[CH_PAD], n); prof(4, &c);
    sample_render(lay[CH_SAMPLE], n); prof(5, &c);
    bool mic_on = S.sel == CH_MIC || S.rec;
    if (mic_on) for (int i = 0; i < n; i++) lay[CH_MIC][i] = in[i];

    mix_ch_t ch[TRACKS];
    for (int i = 0; i < CH_N; i++) {
        float rev = S.mix[i][MX_REV], dly = S.mix[i][MX_DLY];
        if (S.perform && S.sel == i && (i == CH_MIC || i == CH_PAD)) { rev = clampf(rev + S.ex, 0, 1); dly = clampf(dly + S.ey, 0, 1); }
        ch[i] = (mix_ch_t){ S.mix[i][MX_VOL], S.mix[i][MX_PAN], rev, dly,
                            i == CH_MIC ? 120 : i == CH_LEAD ? 100 : i == CH_PAD ? 150 : 0, i == CH_PAD ? -0.2f : 0, 0, 0,
                            S.mute[i] || (i == CH_MIC && !mic_on), i == CH_BASS || i == CH_PAD };
    }
    mix_process(lay, ch, TRACKS, out, n); prof(6, &c);
    prof(7, &c_all);
}

static void status(char *buf, size_t len) { snprintf(buf, len, "%d bpm %s %s", S.bpm, NOTE_NAMES[S.root], sc()->name); }

const fx_t fx_jam = { "JAM", init, process, status, NULL, NULL, true, NULL };

void jam_get_state(jam_state_t *st)
{
    memset(st, 0, sizeof *st);
    st->magic = JAM_MAGIC; st->version = 2; st->smode = S.smode;
    st->bpm = S.bpm; st->style = S.style; st->swing = S.swing; st->root = S.root; st->scale = S.scale;
    st->kit = S.kit; st->bpre = S.bpre; st->lpre = S.lpre; st->ppre = S.ppre;
    for (int i = 0; i < CH_N; i++) st->mute[i] = S.mute[i];
    memcpy(st->mix, S.mix, sizeof st->mix);
    memcpy(st->drum, S.drum, sizeof st->drum);
    memcpy(st->note, S.note, sizeof st->note);
    st->smp_len = S.smp_len;
}
void jam_set_state(const jam_state_t *st) { S.pending = st; jam_cmd(CMD_LOAD, 0, 0, 0); }
float *jam_sample_buf(int *max_len) { if (max_len) *max_len = SAMPLE_MAX; return S.smp; }
uint32_t jam_dirty(void) { return S.dirty; }
uint32_t jam_sample_gen(void) { return S.smp_gen; }

void jam_get_ui(jam_ui_t *u)
{
    memset(u, 0, sizeof *u);
    u->step = (int)(S.pos / S.step_len) % STEPS; u->bpm = S.bpm; u->sel = S.sel; u->perform = S.perform; u->rec = S.rec;
    u->rows = rows_of(S.sel);
    const scale_def_t *s = sc();
    if (S.sel == CH_DRUMS) {
        static const char *L[DRUM_N] = { "K", "S", "H" };
        for (int t = 0; t < DRUM_N; t++) {
            snprintf(u->row_label[DRUM_N - 1 - t], 3, "%s", L[t]);
            for (int st_ = 0; st_ < STEPS; st_++) u->grid[DRUM_N - 1 - t][st_] = S.drum[st_][t] ? 1 : 0;
        }
    } else if (S.sel != CH_MIC) {
        int l = lane(S.sel);
        if (S.sel == CH_SAMPLE && S.smode) u->rows = SLICES;
        for (int r = 0; r < u->rows; r++) {
            int row = u->rows - 1 - r;                           // display top = highest row
            bool sl = S.sel == CH_SAMPLE && S.smode;
            u->root_row[r] = sl ? row == 0 : row % s->n == 0;
            u->row_label[r][0] = (char)('1' + (sl ? row : row % s->n)); u->row_label[r][1] = 0;
            for (int st_ = 0; st_ < STEPS; st_++) { int v = S.note[l][st_]; u->grid[r][st_] = v == row ? 1 : (sl && v == row + REV) ? 2 : 0; }
        }
    }
    for (int i = 0; i < CH_N; i++) { u->mute[i] = S.mute[i]; memcpy(u->mix[i], S.mix[i], sizeof u->mix[i]); }
    snprintf(u->param[P_BPM], 12, "%d", S.bpm);
    snprintf(u->param[P_ROOT], 12, "%s", NOTE_NAMES[S.root]);
    snprintf(u->param[P_SCALE], 12, "%s", s->name);
    snprintf(u->param[P_STYLE], 12, "%s", STYLES[S.style].name);
    snprintf(u->param[P_SWING], 12, "%d%%", S.swing);
    snprintf(u->param[P_KIT], 12, "%s", kit_name(S.kit));
    snprintf(u->param[P_BASS], 12, "%s", fvoice_preset_name(FV_BASS, S.bpre));
    snprintf(u->param[P_LEAD], 12, "%s", fvoice_preset_name(FV_LEAD, S.lpre));
    snprintf(u->param[P_PAD], 12, "%s", synth_preset_name(SK_KEYS, S.ppre));
    snprintf(u->param[P_SMODE], 12, "%s", S.smode ? "SLICE" : "PITCH");
    snprintf(u->param[P_CLEAR], 12, "ALL (tap)");
    snprintf(u->scale, sizeof u->scale, "%s%s", NOTE_NAMES[S.root], s->n == 5 ? "p" : S.scale == 1 ? "m" : "");
    u->mic_db = S.mic_db; u->sample_s = S.smp_len / FS; u->sample_ok = S.smp_len > 0;
    u->gr = mix_gain_reduction();
}
