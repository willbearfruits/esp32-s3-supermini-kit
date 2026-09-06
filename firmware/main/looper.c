// Fixed grid looper. The tempo and bar count are settings, a metronome
// gives the count-in, every take is exactly one loop long and stops by
// itself, so a layer always lands where you played it.
#include "looper.h"
#include "dsp.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "tune.h"

#include <string.h>
#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "sdkconfig.h"

static const char *TAG = "looper";

#define MS(x)        ((int)((x) * 0.001f * FS))
#define CLASSIFY_S   MS(16)            // audio used to classify a hit
#define REFRACT_S    MS(100)           // minimum gap between hits
#define HIT_LAG      MS(5)             // detector lag: a hit is seen one block late plus the DMA
#define NOTE_LAG     MS(30)            // tracker lag on a pitch change within a phrase
#define ONSET_MAX_S  MS(80)            // never date a note-on back further than this

enum { L_BASS, L_CHORDS, L_LEAD, L_N };
static const int page_layer[PG_COUNT] = { -1, L_BASS, L_CHORDS, L_LEAD, -1 };
static const char *page_names[PG_COUNT] = { "DRUMS", "BASS", "CHORDS", "LEAD", "VOCAL" };

static struct {
    int   page;
    bool  armed, rec;
    float bpm, step_len;
    int   bars, steps, loop_len, pos, last_step;

    uint8_t drum_pat[LOOP_MAX_STEPS][DRUM_N];
    uint8_t seq[L_N][LOOP_MAX_STEPS];
    bool    has[PG_COUNT];

    scale_t scale;
    float   pc_w[12];                  // pitch classes heard while recording bass

    synth_t syn_pb[L_N], syn_live[L_N];
    int     live_note;

    uint8_t *vbuf;                     // vocal loop, mu-law, one byte per sample
    int      vcap;

    // beatbox onset detector
    float bg_db, cap_db;
    int   refr, cap_len, cap_pos;
    bool  capturing;
    drum_bands_t cap;

    // metronome
    float click_amp, click_ph, click_hz, click_c;

    volatile int ev_short, ev_long, ev_clear;
    char  msg[24];
    int   msg_samples;
    int   last_drum;
    float drum_lo, drum_hi;
    float live_out[512];
} S;

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

static void live_off(void)
{
    for (int l = 0; l < L_N; l++) synth_note_off(&S.syn_live[l], -1);
    S.live_note = -1;
}

static bool song_empty(void)
{
    for (int p = 0; p < PG_COUNT; p++) if (S.has[p]) return false;
    return true;
}

static void clear_song(void)
{
    S.armed = false; S.rec = false;
    memset(S.drum_pat, 0, sizeof S.drum_pat);
    memset(S.seq, 0, sizeof S.seq);
    memset(S.has, 0, sizeof S.has);
    memset(S.pc_w, 0, sizeof S.pc_w);
    S.scale.locked = false;
    for (int l = 0; l < L_N; l++) synth_note_off(&S.syn_pb[l], -1);
    live_off();
    if (S.vbuf) memset(S.vbuf, 0xFF, S.vcap);
}

static void init(void)
{
    memset(&S, 0, sizeof S);
    S.page = PG_DRUMS; S.live_note = -1; S.last_drum = -1; S.bg_db = -60; S.last_step = -1;
    drums_init();
    tune_init();
    synth_init(&S.syn_pb[L_BASS], &CFG_BASS, 1);   synth_init(&S.syn_live[L_BASS], &CFG_BASS, 1);
    synth_init(&S.syn_pb[L_CHORDS], &CFG_CHORD, 3); synth_init(&S.syn_live[L_CHORDS], &CFG_CHORD, 3);
    synth_init(&S.syn_pb[L_LEAD], &CFG_LEAD, 1);   synth_init(&S.syn_live[L_LEAD], &CFG_LEAD, 1);

    S.bpm = CONFIG_KIT_BPM;
    S.bars = CONFIG_KIT_LOOP_BARS;
    S.steps = S.bars * 16;
    S.step_len = 15.0f * FS / S.bpm;
    S.loop_len = (int)(S.step_len * S.steps + 0.5f);
    S.click_c = expf(-1.0f / (0.012f * FS));

    int cap = CONFIG_KIT_LOOP_MAX_SEC * CONFIG_KIT_SAMPLE_RATE;
    while (cap >= 2 * CONFIG_KIT_SAMPLE_RATE) {
        S.vbuf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
        if (!S.vbuf) S.vbuf = heap_caps_malloc(cap, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (S.vbuf) break;
        cap /= 2;
    }
    S.vcap = S.vbuf ? cap : 0;
    ESP_LOGI(TAG, "%d bpm, %d bars, loop %d ms; vocal buffer %d KB, free heap %u", CONFIG_KIT_BPM,
             S.bars, (int)(S.loop_len * 1000 / FS), S.vcap / 1024, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    clear_song();
}

static inline int step_at(int pos) { int s = (int)lroundf(pos / S.step_len); return ((s % S.steps) + S.steps) % S.steps; }

static void snap_layer(int l)
{
    for (int s = 0; s < S.steps; s++) {
        uint8_t b = S.seq[l][s];
        if (!(b & 0x7F)) continue;
        S.seq[l][s] = (b & SEQ_ATTACK) | scale_snap(&S.scale, (float)(b & 0x7F));
    }
}

// ---- recording: arm, start at the loop start, stop at the next one
static void arm(void)
{
    if (S.page == PG_VOCAL && (!S.vbuf || S.vcap < S.loop_len)) { say("loop too long for vocal"); return; }
    S.armed = true;
    // nothing playing yet: restart the clock one bar out, so the count-in
    // is exactly one bar and the loop begins with the take
    if (song_empty()) { S.pos = S.loop_len - (int)(S.step_len * 16); S.last_step = -1; }
}

static void rec_begin(void)
{
    S.armed = false; S.rec = true;
    switch (S.page) {
    case PG_DRUMS: memset(S.drum_pat, 0, sizeof S.drum_pat); break;
    case PG_VOCAL: memset(S.vbuf, 0xFF, S.loop_len); break;
    default:
        memset(S.seq[page_layer[S.page]], 0, LOOP_MAX_STEPS);
        memset(S.pc_w, 0, sizeof S.pc_w);
        synth_note_off(&S.syn_pb[page_layer[S.page]], -1);
        break;
    }
    S.has[S.page] = false;
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
    S.has[S.page] = true;
    // a take that lands on the loop start keeps its last note ringing into
    // step 0; the sequencer plays step 0 on the next pass anyway
}

static void cancel(void)
{
    S.armed = false;
    if (S.rec) { S.rec = false; say("take cancelled"); }
}

static void handle_events(void)
{
    if (S.ev_clear) { S.ev_clear = 0; S.ev_short = S.ev_long = 0; clear_song(); say("song cleared"); }
    while (S.ev_short) { S.ev_short--; if (S.armed || S.rec) cancel(); else arm(); }
    while (S.ev_long) {
        S.ev_long--;
        cancel();
        live_off();
        S.page = (S.page + 1) % PG_COUNT;
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
    if (l == L_CHORDS) {
        int tri[3];
        scale_triad(&S.scale, note, tri);
        synth_note_off(y, -1);
        for (int i = 0; i < 3; i++) synth_note_on(y, tri[i], 0.9f);
    } else synth_note_on(y, note, 0.9f);
}

static void click(bool accent)
{
    S.click_amp = accent ? 0.5f : 0.3f;
    S.click_hz = accent ? 2000.0f : 1400.0f;
    S.click_ph = 0;
}

static void on_step(int s)
{
    // metronome: while armed or recording, and on the drums page until a
    // beat exists
    bool metro = S.armed || S.rec || (S.page == PG_DRUMS && !S.has[PG_DRUMS]);
    if (metro && (s % 4) == 0) click((s % 16) == 0);

    bool rec_drums = S.rec && S.page == PG_DRUMS;
    if (!rec_drums)
        for (int t = 0; t < DRUM_N; t++)
            if (S.drum_pat[s][t]) drums_trigger(t, S.drum_pat[s][t] / 127.0f);
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

// ---- live voice on the melodic pages
static int page_note(int page, const voice_t *v)
{
    int n = scale_snap(&S.scale, (float)v->note);
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
    if (S.rec) {
        int s = step_at(S.pos - lag);
        S.seq[l][s] = n | SEQ_ATTACK;
    }
}

static void melodic_page(const voice_t *v)
{
    int l = page_layer[S.page];
    int nn = (v->voiced && v->note >= 0) ? page_note(S.page, v) : -1;
    if (nn != S.live_note) {
        if (nn < 0) synth_note_off(&S.syn_live[l], -1);
        else {
            // first note of a phrase: the tracker needed a moment to settle,
            // so date it back to when the voice started
            int lag = S.live_note < 0 ? v->since_onset : NOTE_LAG;
            if (lag > ONSET_MAX_S) lag = ONSET_MAX_S;
            live_note_on(l, nn, lag);
        }
        S.live_note = nn;
    }
    if (S.rec && S.page == PG_BASS && v->voiced && v->note >= 0) S.pc_w[v->note % 12] += 1;
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
            drums_trigger(type, vel);
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
    handle_events();
    if (S.msg_samples > 0 && (S.msg_samples -= n) <= 0) S.msg[0] = 0;

    // transport: the loop start is where takes begin and end
    if (S.pos == 0 || S.last_step < 0) {
        if (S.rec) rec_end();
        if (S.armed) rec_begin();
    }
    int s = (int)(S.pos / S.step_len);
    if (s >= S.steps) s = S.steps - 1;
    if (s != S.last_step) { on_step(s); S.last_step = s; }

    // live input
    switch (S.page) {
    case PG_DRUMS: drums_page(in, n, v); break;
    case PG_VOCAL: tune_process(in, S.live_out, n, v, &S.scale); S.live_note = tune_target(); break;
    default:       melodic_page(v); break;
    }

    // render
    memset(out, 0, n * sizeof(float));
    drums_render(out, n, 0.8f);
    for (int l = 0; l < L_N; l++) { synth_render(&S.syn_pb[l], out, n); synth_render(&S.syn_live[l], out, n); }
    bool vocal_rec = S.rec && S.page == PG_VOCAL;
    if (S.vbuf && (S.has[PG_VOCAL] || vocal_rec)) {
        for (int i = 0; i < n; i++) {
            int p = S.pos + i;
            if (p >= S.loop_len) p -= S.loop_len;
            if (vocal_rec) S.vbuf[p] = mulaw_enc(S.live_out[i]);
            else out[i] += mulaw_dec(S.vbuf[p]) * 0.9f;
        }
    }
    if (S.page == PG_VOCAL) for (int i = 0; i < n; i++) out[i] += S.live_out[i] * 0.9f;
    if (S.click_amp > 0.001f) {
        const float inc = S.click_hz / FS;
        for (int i = 0; i < n; i++) {
            out[i] += fast_sin01(S.click_ph) * S.click_amp;
            S.click_ph += inc; if (S.click_ph >= 1) S.click_ph -= 1;
            S.click_amp *= S.click_c;
        }
    }

    // advance; a take that wraps mid-block ends at the next block start
    S.pos += n;
    if (S.pos >= S.loop_len) S.pos -= S.loop_len;
    if (S.pos < n && S.pos != 0) S.pos = 0;   // land exactly on the loop start
}

static void status(char *buf, size_t len)
{
    snprintf(buf, len, "%s%s", page_names[S.page], S.rec ? " REC" : "");
}

const fx_t fx_looper = { "LOOPER", init, process, status, NULL, NULL };

// ---- UI side
void looper_event(int ev)
{
    if (ev == LP_SHORT) S.ev_short++;
    else if (ev == LP_LONG) S.ev_long++;
    else S.ev_clear++;
}

void looper_get_ui(looper_ui_t *u)
{
    u->page = S.page; u->armed = S.armed; u->rec = S.rec; u->bpm = S.bpm;
    u->bars = S.bars; u->steps = S.steps; u->step = S.last_step < 0 ? 0 : S.last_step;
    u->beats_to_go = S.armed ? (S.loop_len - S.pos + (int)(S.step_len * 4) - 1) / (int)(S.step_len * 4) : 0;
    memcpy(u->has, S.has, sizeof u->has);
    scale_name(&S.scale, u->key, sizeof u->key);
    u->live_note = S.live_note; u->last_drum = S.last_drum; u->drum_lo = S.drum_lo; u->drum_hi = S.drum_hi;
    memcpy(u->msg, S.msg, sizeof u->msg);
}

const uint8_t *looper_drum_pattern(void) { return &S.drum_pat[0][0]; }
const uint8_t *looper_seq(int page) { int l = page_layer[page]; return l < 0 ? NULL : S.seq[l]; }
const char *looper_page_name(int page) { return page_names[page]; }
