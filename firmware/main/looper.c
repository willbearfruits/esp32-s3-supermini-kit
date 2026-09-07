#include "looper.h"
#include "dsp.h"
#include "scale.h"
#include "synth.h"
#include "kit.h"
#include "tune.h"
#include "mix.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "sdkconfig.h"
#include "esp_cpu.h"

static const char *TAG = "looper";

#define MS(x)        ((int)((x) * 0.001f * FS))
#define CLASSIFY_S   MS(16)
#define REFRACT_S    MS(100)
#define HIT_LAG      MS(5)
#define NOTE_LAG     MS(30)
#define ONSET_MAX_S  MS(80)
#define BLK          64

enum { L_BASS, L_CHORDS, L_LEAD, L_N };
static const int page_layer[PG_COUNT] = { -1, L_BASS, L_CHORDS, L_LEAD, -1 };
static const char *page_names[PG_COUNT] = { "DRUMS", "BASS", "CHORDS", "LEAD", "VOCAL" };

typedef struct { bool has, mute; int vol, rev, dly; } layer_t;

static struct {
    int   page;
    bool  armed, rec, replace_next;
    float bpm, step_len;
    int   bars, steps, loop_len, pos, last_step;
    int   swing, human, kit;

    uint8_t drum_pat[LOOP_MAX_STEPS][DRUM_N];
    uint8_t seq[L_N][LOOP_MAX_STEPS];
    layer_t lay[PG_COUNT];

    // undo: the layer as it was before the last take
    int     bk_page;
    bool    bk_has;
    uint8_t bk_drum[LOOP_MAX_STEPS][DRUM_N];
    uint8_t bk_seq[LOOP_MAX_STEPS];
    uint8_t *bk_vocal;

    scale_t scale;
    float   pc_w[12];

    drums_t drums;
    synth_t syn_pb[L_N], syn_live[L_N];
    int     live_note;
    float   expr_cut, expr_bend, expr_vib, expr_space;

    uint8_t *vbuf;
    int      vcap;

    float bg_db, cap_db;
    int   refr, cap_len, cap_pos;
    bool  capturing;
    drum_bands_t cap;

    struct { int left; uint8_t type, vel; } pend[8];   // humanised drum triggers
    uint32_t rnd;

    float click_amp, click_ph, click_hz, click_c;
    void (*clock_cb)(void);
    int   clock_next;

    // bounce
    int      bounce_pass;      // 0 off, 1/2 armed or running
    bool     bounce_running, bounce_done;
    int16_t *bounce[3];
    int      bounce_pos;

    volatile int act_q[8], act_n;
    volatile int dirty;
    char  msg[24];
    int   msg_samples;
    int   last_drum;
    float drum_lo, drum_hi;
    float peak;
    float lay_buf[PG_COUNT][BLK], live_out[BLK];
    uint32_t prof[4];          // cycles: live input, render, mix, rest
} S;
uint32_t *looper_prof(void) { return S.prof; }

static const synth_cfg_t CFG_BASS = { .a_ms = 4, .d_ms = 180, .sus = 0.7f, .r_ms = 60,
    .cutoff = 150, .env_hz = 900, .vel_hz = 300, .q = 1.2f, .detune_cents = 4, .sub = 0.6f, .square = false,
    .glide_ms = 25, .gain = 0.45f };
static const synth_cfg_t CFG_CHORD = { .a_ms = 30, .d_ms = 400, .sus = 0.8f, .r_ms = 250,
    .cutoff = 400, .env_hz = 1500, .vel_hz = 500, .q = 0.8f, .detune_cents = 9, .sub = 0, .square = false,
    .glide_ms = 0, .gain = 0.16f };
static const synth_cfg_t CFG_LEAD = { .a_ms = 6, .d_ms = 150, .sus = 0.75f, .r_ms = 120,
    .cutoff = 800, .env_hz = 2500, .vel_hz = 800, .q = 1.5f, .detune_cents = 12, .sub = 0.25f, .square = true,
    .glide_ms = 40, .gain = 0.3f };

// ---- mu-law
static inline uint8_t mulaw_enc(float x)
{
    int s = (int)(clampf(x, -1.0f, 1.0f) * 32767.0f);
    int sign = s < 0 ? 0x80 : 0;
    if (s < 0) s = -s;
    s += 132;
    if (s > 32767) s = 32767;
    int exp = 7;
    for (int m = 0x4000; (s & m) == 0 && exp > 0; m >>= 1) exp--;
    int mant = (s >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | mant);
}
static inline float mulaw_dec(uint8_t u)
{
    u = ~u;
    int exp = (u >> 4) & 7, mant = u & 0x0F;
    int s = ((mant << 3) + 132) << exp;
    s -= 132;
    return (float)(u & 0x80 ? -s : s) / 32767.0f;
}

static void say(const char *m) { snprintf(S.msg, sizeof S.msg, "%s", m); S.msg_samples = MS(2500); }
static void mark(int bits) { S.dirty |= bits; }

static void live_off(void)
{
    for (int l = 0; l < L_N; l++) synth_note_off(&S.syn_live[l], -1);
    S.live_note = -1;
}

static bool song_empty(void)
{
    for (int p = 0; p < PG_COUNT; p++) if (S.lay[p].has) return false;
    return true;
}

static void set_grid(float bpm, int bars)
{
    S.bpm = bpm; S.bars = bars; S.steps = bars * 16;
    S.step_len = 15.0f * FS / S.bpm;
    S.loop_len = (int)(S.step_len * S.steps + 0.5f);
    if (S.loop_len > S.vcap) S.loop_len = S.vcap;
    if (S.pos >= S.loop_len) S.pos = 0;
    S.last_step = -1;
    S.clock_next = 0;
    mix_set_tempo(bpm);
}

static void clear_layer(int p)
{
    if (p == PG_DRUMS) memset(S.drum_pat, 0, sizeof S.drum_pat);
    else if (p == PG_VOCAL) { if (S.vbuf) memset(S.vbuf, 0xFF, S.loop_len); }
    else { memset(S.seq[page_layer[p]], 0, LOOP_MAX_STEPS); synth_note_off(&S.syn_pb[page_layer[p]], -1); }
    S.lay[p].has = false;
    mark(DIRTY_STATE | (p == PG_VOCAL ? DIRTY_VOCAL : 0));
}

static void clear_song(void)
{
    S.armed = false; S.rec = false;
    for (int p = 0; p < PG_COUNT; p++) { clear_layer(p); S.lay[p].mute = false; }
    memset(S.pc_w, 0, sizeof S.pc_w);
    S.scale.locked = false;
    S.bk_page = -1;
    live_off();
}

static void init(void)
{
    memset(&S, 0, sizeof S);
    S.page = PG_DRUMS; S.live_note = -1; S.last_drum = -1; S.bg_db = -60; S.last_step = -1; S.bk_page = -1;
    S.rnd = 777; S.expr_cut = 1;
    S.swing = 0; S.human = 20; S.kit = 0;
    drums_analyser_init();
    drums_init(&S.drums, 0);
    tune_init();
    mix_init();
    synth_init(&S.syn_pb[L_BASS], &CFG_BASS, 1);   synth_init(&S.syn_live[L_BASS], &CFG_BASS, 1);
    synth_init(&S.syn_pb[L_CHORDS], &CFG_CHORD, 3); synth_init(&S.syn_live[L_CHORDS], &CFG_CHORD, 3);
    synth_init(&S.syn_pb[L_LEAD], &CFG_LEAD, 1);   synth_init(&S.syn_live[L_LEAD], &CFG_LEAD, 1);
    for (int p = 0; p < PG_COUNT; p++) { S.lay[p].vol = 80; S.lay[p].rev = p == PG_DRUMS ? 10 : 25; S.lay[p].dly = p == PG_LEAD || p == PG_VOCAL ? 25 : 0; }
    S.click_c = expf(-1.0f / (0.012f * FS));

    int cap = CONFIG_KIT_LOOP_MAX_SEC * CONFIG_KIT_SAMPLE_RATE;
    while (cap >= 2 * CONFIG_KIT_SAMPLE_RATE) {
        S.vbuf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
        if (S.vbuf) S.bk_vocal = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
        if (S.vbuf && S.bk_vocal) break;
        if (S.vbuf) { free(S.vbuf); S.vbuf = NULL; }
        cap /= 2;
    }
    S.vcap = S.vbuf ? cap : 0;
    set_grid(CONFIG_KIT_BPM, CONFIG_KIT_LOOP_BARS);
    ESP_LOGI(TAG, "%d bpm, %d bars, loop %d ms; vocal buffer %d KB, free heap %u", CONFIG_KIT_BPM,
             S.bars, (int)(S.loop_len * 1000 / FS), S.vcap / 1024, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    clear_song();
    S.dirty = 0;
}

static inline int step_at(int pos) { int s = (int)lroundf(pos / S.step_len); return ((s % S.steps) + S.steps) % S.steps; }
static inline float rnd01(void) { return (frand(&S.rnd) + 1.0f) * 0.5f; }

static void snap_layer(int l)
{
    for (int s = 0; s < S.steps; s++) {
        uint8_t b = S.seq[l][s];
        if (!(b & 0x7F)) continue;
        S.seq[l][s] = (b & SEQ_ATTACK) | scale_snap(&S.scale, (float)(b & 0x7F));
    }
}

// ---- takes
static void backup_layer(void)
{
    int p = S.page;
    S.bk_page = p; S.bk_has = S.lay[p].has;
    if (p == PG_DRUMS) memcpy(S.bk_drum, S.drum_pat, sizeof S.drum_pat);
    else if (p == PG_VOCAL) { if (S.vbuf) memcpy(S.bk_vocal, S.vbuf, S.loop_len); }
    else memcpy(S.bk_seq, S.seq[page_layer[p]], LOOP_MAX_STEPS);
}

static void undo(void)
{
    int p = S.bk_page;
    if (p < 0) { say("nothing to undo"); return; }
    if (p == PG_DRUMS) memcpy(S.drum_pat, S.bk_drum, sizeof S.drum_pat);
    else if (p == PG_VOCAL) { if (S.vbuf) memcpy(S.vbuf, S.bk_vocal, S.loop_len); }
    else memcpy(S.seq[page_layer[p]], S.bk_seq, LOOP_MAX_STEPS);
    S.lay[p].has = S.bk_has;
    S.bk_page = -1;
    say("undone");
    mark(DIRTY_STATE | (p == PG_VOCAL ? DIRTY_VOCAL : 0));
}

static void arm(void)
{
    if (S.page == PG_VOCAL && (!S.vbuf || S.vcap < S.loop_len)) { say("no vocal memory"); return; }
    S.armed = true;
    if (song_empty()) { S.pos = S.loop_len - (int)(S.step_len * 16); S.last_step = -1; }
}

static void rec_begin(void)
{
    S.armed = false; S.rec = true;
    backup_layer();
    bool overdub = S.lay[S.page].has && !S.replace_next;
    S.replace_next = false;
    if (!overdub) {
        if (S.page == PG_DRUMS) memset(S.drum_pat, 0, sizeof S.drum_pat);
        else if (S.page == PG_VOCAL) memset(S.vbuf, 0xFF, S.loop_len);
        else memset(S.seq[page_layer[S.page]], 0, LOOP_MAX_STEPS);
    }
    if (S.page == PG_BASS) memset(S.pc_w, 0, sizeof S.pc_w);
    if (page_layer[S.page] >= 0) synth_note_off(&S.syn_pb[page_layer[S.page]], -1);
}

static void rec_end(void)
{
    S.rec = false;
    if (S.page == PG_BASS && !S.scale.locked) {
        float total = 0;
        for (int i = 0; i < 12; i++) total += S.pc_w[i];
        if (total > 10 && scale_detect(S.pc_w, &S.scale)) {
            snap_layer(L_BASS);
            char nm[10];
            ESP_LOGI(TAG, "key locked: %s", scale_name(&S.scale, nm, sizeof nm));
        }
    }
    S.lay[S.page].has = true;
    mark(DIRTY_STATE | (S.page == PG_VOCAL ? DIRTY_VOCAL : 0));
}

static void cancel(void)
{
    S.armed = false;
    if (S.rec) { S.rec = false; undo(); say("take cancelled"); }
}

static void do_action(int a)
{
    switch (a) {
    case ACT_TAP:        if (S.armed || S.rec) cancel(); else arm(); break;
    case ACT_CANCEL:     cancel(); break;
    case ACT_NEXT_PAGE:  cancel(); live_off(); S.page = (S.page + 1) % PG_COUNT; mark(DIRTY_STATE); break;
    case ACT_PREV_PAGE:  cancel(); live_off(); S.page = (S.page + PG_COUNT - 1) % PG_COUNT; mark(DIRTY_STATE); break;
    case ACT_UNDO:       undo(); break;
    case ACT_CLEAR_LAYER: backup_layer(); clear_layer(S.page); say("layer cleared"); break;
    case ACT_MUTE:       S.lay[S.page].mute = !S.lay[S.page].mute; mark(DIRTY_STATE); break;
    case ACT_CLEAR_SONG: clear_song(); say("song cleared"); break;
    case ACT_REPLACE:    S.replace_next = !S.replace_next; say(S.replace_next ? "next take replaces" : "next take overdubs"); break;
    }
}

static void handle_actions(void)
{
    while (S.act_n > 0) {
        int a = S.act_q[0];
        for (int i = 1; i < S.act_n; i++) S.act_q[i - 1] = S.act_q[i];
        S.act_n--;
        do_action(a);
    }
}

// ---- sequencer
static void play_layer(int l, int s)
{
    uint8_t b = S.seq[l][s];
    int note = b & 0x7F;
    synth_t *y = &S.syn_pb[l];
    if (!note) { synth_note_off(y, -1); return; }
    if (!(b & SEQ_ATTACK)) return;
    float vel = 0.9f - S.human * 0.002f * rnd01();
    if (l == L_CHORDS) {
        int tri[3];
        scale_triad(&S.scale, note, tri);
        synth_note_off(y, -1);
        for (int i = 0; i < 3; i++) synth_note_on(y, tri[i], vel);
    } else synth_note_on(y, note, vel);
}

static void click(bool accent)
{
    S.click_amp = accent ? 0.5f : 0.3f;
    S.click_hz = accent ? 2000.0f : 1400.0f;
    S.click_ph = 0;
}

static void drum_hit(int t, float vel)
{
    drums_trigger(&S.drums, t, vel);
    if (t == DRUM_KICK) mix_kick();
}

static void on_step(int s)
{
    bool metro = S.armed || S.rec;
    if (metro && (s % 4) == 0) click((s % 16) == 0);

    bool rec_drums = S.rec && S.page == PG_DRUMS;
    if (!rec_drums)
        for (int t = 0; t < DRUM_N; t++)
            if (S.drum_pat[s][t]) {
                float vel = S.drum_pat[s][t] / 127.0f * (1.0f - S.human * 0.003f * rnd01());
                int delay = S.human > 0 ? (int)(rnd01() * S.human * 0.04f) : 0;   // up to 4 blocks (8 ms)
                if (delay == 0) drum_hit(t, vel);
                else for (int i = 0; i < 8; i++) if (S.pend[i].left == 0) { S.pend[i].left = delay; S.pend[i].type = t; S.pend[i].vel = (uint8_t)(vel * 127); break; }
            }
    for (int l = 0; l < L_N; l++) {
        bool rec_this = S.rec && page_layer[S.page] == l;
        if (rec_this) {
            if (S.live_note >= 0) {
                uint8_t b = S.seq[l][s];
                if (!((b & SEQ_ATTACK) && (b & 0x7F) == S.live_note)) S.seq[l][s] = S.live_note;
            }
        } else play_layer(l, s);
    }
}

// ---- live voice
static int page_note(int page, int note)
{
    int n = scale_snap(&S.scale, (float)note);
    switch (page) {
    case PG_BASS:   while (n > 52) n -= 12; while (n < 28) n += 12; break;
    case PG_CHORDS: while (n > 64) n -= 12; while (n < 48) n += 12; break;
    default:        while (n > 88) n -= 12; while (n < 45) n += 12; break;
    }
    return n;
}

static void live_note_on(int l, int n, int lag)
{
    synth_t *y = &S.syn_live[l];
    if (l == L_CHORDS) {
        int tri[3];
        scale_triad(&S.scale, n, tri);
        synth_note_off(y, -1);
        for (int i = 0; i < 3; i++) synth_note_on(y, tri[i], 0.9f);
    } else synth_note_on(y, n, 0.9f);
    if (S.rec) S.seq[l][step_at(S.pos - lag)] = n | SEQ_ATTACK;
}

static void melodic_page(const voice_t *v)
{
    int l = page_layer[S.page];
    int nn = (v->voiced && v->note >= 0) ? page_note(S.page, v->note) : -1;
    if (nn != S.live_note) {
        if (nn < 0) synth_note_off(&S.syn_live[l], -1);
        else {
            int lag = S.live_note < 0 ? v->since_onset : NOTE_LAG;
            if (lag > ONSET_MAX_S) lag = ONSET_MAX_S;
            live_note_on(l, nn, lag);
        }
        S.live_note = nn;
    }
    if (S.rec && S.page == PG_BASS && v->voiced && v->note >= 0) S.pc_w[v->note % 12] += 1;
}

void looper_midi_note(int note, int vel, bool on)
{
    if (S.page == PG_DRUMS) {
        if (!on) return;
        int t = note == 36 || note == 35 ? DRUM_KICK : (note == 42 || note == 44 || note == 46 ? DRUM_HAT : DRUM_SNARE);
        drum_hit(t, vel / 127.0f);
        if (S.rec) { int s = step_at(S.pos); uint8_t v7 = vel; if (v7 > S.drum_pat[s][t]) S.drum_pat[s][t] = v7; }
        return;
    }
    if (S.page == PG_VOCAL) return;
    int l = page_layer[S.page];
    if (on) { int n = page_note(S.page, note); live_note_on(l, n, 0); S.live_note = n; }
    else { synth_note_off(&S.syn_live[l], -1); S.live_note = -1; }
}

// ---- beatbox
static void drums_page(const float *in, int n, const voice_t *v)
{
    drum_bands_t b;
    drums_analyse(in, n, &b);
    if (S.refr > 0) S.refr -= n;
    const float thr = voice_noise_db() + CONFIG_KIT_ONSET_DB;
    if (!S.capturing && S.refr <= 0 && v->db > thr && v->db > S.bg_db + 8.0f) {
        S.capturing = true; S.cap_len = 0;
        S.cap_pos = S.pos - HIT_LAG;
        S.cap_db = v->db; S.cap.lo = S.cap.hi = S.cap.all = 0; S.refr = REFRACT_S;
    }
    const float blk = (float)n / FS;
    S.bg_db += (1 - expf(-blk / (v->db > S.bg_db ? 0.004f : 0.05f))) * (v->db - S.bg_db);
    if (S.capturing) {
        S.cap.lo += b.lo; S.cap.hi += b.hi; S.cap.all += b.all;
        if (v->db > S.cap_db) S.cap_db = v->db;
        S.cap_len += n;
        if (S.cap_len >= CLASSIFY_S) {
            S.capturing = false;
            int type = drums_classify(&S.cap, &S.drum_lo, &S.drum_hi);
            float vel = clampf(0.35f + (S.cap_db - thr) / 25.0f, 0.35f, 1.0f);
            drum_hit(type, vel);
            S.last_drum = type;
            if (S.rec) {
                uint8_t v7 = (uint8_t)(vel * 127);
                int s = step_at(S.cap_pos);
                if (v7 > S.drum_pat[s][type]) S.drum_pat[s][type] = v7;
            }
        }
    }
}

static void IRAM_ATTR process(const float *in, float *out, int n, const voice_t *v)
{
    handle_actions();
    if (S.msg_samples > 0 && (S.msg_samples -= n) <= 0) S.msg[0] = 0;

    // loop start: takes begin and end here, bounces start here
    if (S.pos == 0 || S.last_step < 0) {
        if (S.rec) rec_end();
        if (S.armed) rec_begin();
        if (S.bounce_pass && !S.bounce_running && !S.bounce_done) { S.bounce_running = true; S.bounce_pos = 0; }
    }
    // step with swing: odd 16ths are late by up to a third of a step
    int s = (int)(S.pos / S.step_len);
    if (s >= S.steps) s = S.steps - 1;
    if ((s & 1) && S.pos - s * S.step_len < S.swing * 0.01f * S.step_len / 3.0f) s--;
    if (s != S.last_step) { on_step(s); S.last_step = s; }
    // humanised drum triggers
    for (int i = 0; i < 8; i++) if (S.pend[i].left > 0 && --S.pend[i].left == 0) drum_hit(S.pend[i].type, S.pend[i].vel / 127.0f);
    // MIDI clock, 24 per beat
    if (S.clock_cb) {
        int tick = (int)(S.step_len * 4 / 24);
        while (S.pos >= S.clock_next) { S.clock_cb(); S.clock_next += tick; }
        if (S.pos + n >= S.loop_len) S.clock_next = 0;
    }

    uint32_t c0 = esp_cpu_get_cycle_count();
    // live input
    switch (S.page) {
    case PG_DRUMS: drums_page(in, n, v); break;
    case PG_VOCAL: tune_process(in, S.live_out, n, v, &S.scale); S.live_note = tune_target(); break;
    default:       melodic_page(v); break;
    }
    for (int l = 0; l < L_N; l++) synth_set_expr(&S.syn_live[l], S.expr_bend, S.expr_cut, S.expr_vib);

    uint32_t c1 = esp_cpu_get_cycle_count();
    // render each layer dry
    float *lay[PG_COUNT];
    for (int p = 0; p < PG_COUNT; p++) { lay[p] = S.lay_buf[p]; memset(lay[p], 0, n * sizeof(float)); }
    drums_render(&S.drums, lay[PG_DRUMS], n, 0.8f);
    for (int l = 0; l < L_N; l++) { synth_render(&S.syn_pb[l], lay[l + 1], n); synth_render(&S.syn_live[l], lay[l + 1], n); }
    bool vocal_rec = S.rec && S.page == PG_VOCAL;
    if (S.vbuf && (S.lay[PG_VOCAL].has || vocal_rec)) {
        for (int i = 0; i < n; i++) {
            int p = S.pos + i;
            if (p >= S.loop_len) p -= S.loop_len;
            if (vocal_rec) {
                float old = S.lay[PG_VOCAL].has ? mulaw_dec(S.vbuf[p]) : 0;   // overdub sums
                S.vbuf[p] = mulaw_enc(old + S.live_out[i]);
            } else lay[PG_VOCAL][i] += mulaw_dec(S.vbuf[p]) * 0.9f;
        }
    }
    if (S.page == PG_VOCAL) for (int i = 0; i < n; i++) lay[PG_VOCAL][i] += S.live_out[i] * 0.9f;

    uint32_t c2 = esp_cpu_get_cycle_count();
    // mix
    mix_ch_t ch[MIX_N];
    for (int p = 0; p < PG_COUNT; p++) {
        ch[p].vol = S.lay[p].vol * 0.01f; ch[p].mute = S.lay[p].mute;
        ch[p].rev = S.lay[p].rev * 0.01f; ch[p].dly = S.lay[p].dly * 0.01f;
        if (p == S.page && S.expr_space > ch[p].rev) ch[p].rev = S.expr_space;
    }
    mix_process(lay, ch, out, n);
    uint32_t c3 = esp_cpu_get_cycle_count();
    S.prof[0] += (c1 - c0) >> 4; S.prof[1] += (c2 - c1) >> 4; S.prof[2] += (c3 - c2) >> 4;
    if (S.click_amp > 0.001f) {
        const float inc = S.click_hz / FS;
        for (int i = 0; i < n; i++) {
            out[i] += fast_sin01(S.click_ph) * S.click_amp;
            S.click_ph += inc; if (S.click_ph >= 1) S.click_ph -= 1;
            S.click_amp *= S.click_c;
        }
    }
    for (int i = 0; i < n; i++) { float a = fabsf(out[i]); if (a > S.peak) S.peak = a; }

    // bounce capture
    if (S.bounce_running) {
        int first = S.bounce_pass == 1 ? 0 : 3;
        for (int i = 0; i < n && S.bounce_pos + i < S.loop_len; i++) {
            for (int k = 0; k < 3; k++) {
                int p = first + k;
                float x = p < PG_COUNT ? (S.lay[p].mute ? 0 : lay[p][i] * S.lay[p].vol * 0.01f) : out[i];
                S.bounce[k][S.bounce_pos + i] = (int16_t)(clampf(x, -1, 1) * 32767);
            }
        }
        S.bounce_pos += n;
        if (S.bounce_pos >= S.loop_len) { S.bounce_running = false; S.bounce_done = true; }
    }

    S.pos += n;
    if (S.pos >= S.loop_len) S.pos -= S.loop_len;
    if (S.pos < n && S.pos != 0) S.pos = 0;
}

static void status(char *buf, size_t len) { snprintf(buf, len, "%s%s", page_names[S.page], S.rec ? " REC" : ""); }

const fx_t fx_looper = { "LOOPER", init, process, status, NULL, NULL };

// ---- UI side ---------------------------------------------------------------
void looper_action(int act) { if (S.act_n < 8) S.act_q[S.act_n++] = act; }

static const char *PNAME[PR_N] = { "VOLUME", "REVERB", "DELAY", "SWING", "HUMANISE", "PUMP", "DRIVE", "KIT", "TEMPO", "BARS" };
const char *looper_param_name(int p) { return p >= 0 && p < PR_N ? PNAME[p] : "?"; }

int looper_param_get(int p)
{
    switch (p) {
    case PR_VOL:   return S.lay[S.page].vol;
    case PR_REV:   return S.lay[S.page].rev;
    case PR_DLY:   return S.lay[S.page].dly;
    case PR_SWING: return S.swing;
    case PR_HUMAN: return S.human;
    case PR_PUMP:  return (int)(mix_params()->pump * 100 + 0.5f);
    case PR_DRIVE: return (int)(mix_params()->drive * 10 + 0.5f);
    case PR_KIT:   return S.kit;
    case PR_BPM:   return (int)(S.bpm + 0.5f);
    case PR_BARS:  return S.bars;
    }
    return 0;
}

void looper_param_set(int p, int v)
{
    switch (p) {
    case PR_VOL:   S.lay[S.page].vol = v < 0 ? 0 : (v > 100 ? 100 : v); break;
    case PR_REV:   S.lay[S.page].rev = v < 0 ? 0 : (v > 100 ? 100 : v); break;
    case PR_DLY:   S.lay[S.page].dly = v < 0 ? 0 : (v > 100 ? 100 : v); break;
    case PR_SWING: S.swing = v < 0 ? 0 : (v > 100 ? 100 : v); break;
    case PR_HUMAN: S.human = v < 0 ? 0 : (v > 100 ? 100 : v); break;
    case PR_PUMP:  mix_params()->pump = clampf(v * 0.01f, 0, 1); break;
    case PR_DRIVE: mix_params()->drive = clampf(v * 0.1f, 1, 4); break;
    case PR_KIT:   S.kit = ((v % KIT_N) + KIT_N) % KIT_N; drums_set_kit(&S.drums, S.kit); S.kit = S.drums.kit; break;
    case PR_BPM:   if (v < 40) v = 40; if (v > 240) v = 240; if (v != (int)(S.bpm + 0.5f)) { set_grid(v, S.bars); if (S.lay[PG_VOCAL].has) clear_layer(PG_VOCAL); } break;
    case PR_BARS:  if (v < 1) v = 1; if (v > LOOP_MAX_BARS) v = LOOP_MAX_BARS; if (v != S.bars) { set_grid(S.bpm, v); if (S.lay[PG_VOCAL].has) clear_layer(PG_VOCAL); } break;
    }
    mark(DIRTY_STATE);
}

void looper_param_step(int p, int dir)
{
    int step = (p == PR_KIT || p == PR_BPM || p == PR_BARS) ? 1 : 5;
    if (p == PR_BARS) { int b = S.bars; b = dir > 0 ? (b < 8 ? b * 2 : 8) : (b > 1 ? b / 2 : 1); looper_param_set(p, b); return; }
    looper_param_set(p, looper_param_get(p) + dir * step);
}

void looper_param_text(int p, char *buf, int len)
{
    int v = looper_param_get(p);
    switch (p) {
    case PR_KIT:   snprintf(buf, len, "%s", kit_name(v)); break;
    case PR_DRIVE: snprintf(buf, len, "%d.%d", v / 10, v % 10); break;
    case PR_BPM:   snprintf(buf, len, "%d bpm", v); break;
    case PR_BARS:  snprintf(buf, len, "%d bar%s", v, v > 1 ? "s" : ""); break;
    default:       snprintf(buf, len, "%d%%", v); break;
    }
}

void looper_expr(float cut, float bend, float vib, float space)
{
    S.expr_cut = cut; S.expr_bend = bend; S.expr_vib = vib; S.expr_space = space;
}

void looper_get_ui(looper_ui_t *u)
{
    u->page = S.page; u->armed = S.armed; u->rec = S.rec; u->replace = S.replace_next; u->bpm = S.bpm;
    u->bars = S.bars; u->steps = S.steps; u->step = S.last_step < 0 ? 0 : S.last_step;
    u->beat = u->step / 4;
    u->beats_to_go = S.armed ? (S.loop_len - S.pos + (int)(S.step_len * 4) - 1) / (int)(S.step_len * 4) : 0;
    for (int p = 0; p < PG_COUNT; p++) { u->has[p] = S.lay[p].has; u->mute[p] = S.lay[p].mute; }
    scale_name(&S.scale, u->key, sizeof u->key);
    u->live_note = S.live_note; u->last_drum = S.last_drum; u->drum_lo = S.drum_lo; u->drum_hi = S.drum_hi;
    memcpy(u->msg, S.msg, sizeof u->msg);
    u->level = S.peak; S.peak = 0;
    u->gr = mix_gain_reduction();
    u->bounce = S.bounce_pass;
    u->undo_page = S.bk_page;
}

const uint8_t *looper_drum_pattern(void) { return &S.drum_pat[0][0]; }
const uint8_t *looper_seq(int page) { int l = page_layer[page]; return l < 0 ? NULL : S.seq[l]; }
const char *looper_page_name(int page) { return page_names[page]; }

// ---- persistence
int looper_take_dirty(void) { int d = S.dirty; S.dirty = 0; return d; }

void looper_get_state(song_state_t *st)
{
    memset(st, 0, sizeof *st);
    st->magic = SONG_MAGIC;
    st->bpm10 = (uint16_t)(S.bpm * 10 + 0.5f); st->bars = S.bars; st->page = S.page;
    st->swing = S.swing; st->human = S.human; st->pump = (uint8_t)(mix_params()->pump * 100 + 0.5f);
    st->drive10 = (uint8_t)(mix_params()->drive * 10 + 0.5f); st->kit = S.kit;
    st->root = S.scale.root; st->minor = S.scale.minor; st->scale_locked = S.scale.locked;
    for (int p = 0; p < PG_COUNT; p++) {
        st->layer[p].has = S.lay[p].has; st->layer[p].mute = S.lay[p].mute;
        st->layer[p].vol = S.lay[p].vol; st->layer[p].rev = S.lay[p].rev; st->layer[p].dly = S.lay[p].dly;
    }
    memcpy(st->drum_pat, S.drum_pat, sizeof st->drum_pat);
    memcpy(st->seq, S.seq, sizeof st->seq);
}

void looper_set_state(const song_state_t *st)
{
    if (st->magic != SONG_MAGIC) return;
    set_grid(st->bpm10 / 10.0f, st->bars < 1 ? 1 : (st->bars > LOOP_MAX_BARS ? LOOP_MAX_BARS : st->bars));
    S.page = st->page < PG_COUNT ? st->page : 0;
    S.swing = st->swing; S.human = st->human;
    mix_params()->pump = st->pump * 0.01f; mix_params()->drive = clampf(st->drive10 * 0.1f, 1, 4);
    S.kit = st->kit; drums_set_kit(&S.drums, S.kit); S.kit = S.drums.kit;
    S.scale.root = st->root; S.scale.minor = st->minor; S.scale.locked = st->scale_locked;
    for (int p = 0; p < PG_COUNT; p++) {
        S.lay[p].has = st->layer[p].has; S.lay[p].mute = st->layer[p].mute;
        S.lay[p].vol = st->layer[p].vol; S.lay[p].rev = st->layer[p].rev; S.lay[p].dly = st->layer[p].dly;
    }
    memcpy(S.drum_pat, st->drum_pat, sizeof S.drum_pat);
    memcpy(S.seq, st->seq, sizeof S.seq);
    S.lay[PG_VOCAL].has = false;      // until the vocal file is loaded
    S.dirty = 0;
}

uint8_t *looper_vocal_buf(void) { return S.vbuf; }
int looper_loop_len(void) { return S.loop_len; }
void looper_vocal_loaded(void) { S.lay[PG_VOCAL].has = true; }
void looper_set_clock_cb(void (*cb)(void)) { S.clock_cb = cb; }

// ---- bounce
bool looper_bounce_start(int pass)
{
    if (S.bounce_pass) return false;
    for (int k = 0; k < 3; k++) {
        S.bounce[k] = heap_caps_malloc(S.loop_len * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (!S.bounce[k]) { looper_bounce_release(); return false; }
    }
    S.bounce_done = false; S.bounce_running = false; S.bounce_pass = pass;
    return true;
}
bool looper_bounce_done(void) { return S.bounce_done; }
int16_t *looper_bounce_buf(int i) { return S.bounce[i]; }
void looper_bounce_release(void)
{
    for (int k = 0; k < 3; k++) { if (S.bounce[k]) free(S.bounce[k]); S.bounce[k] = NULL; }
    S.bounce_pass = 0; S.bounce_done = false; S.bounce_running = false;
}
