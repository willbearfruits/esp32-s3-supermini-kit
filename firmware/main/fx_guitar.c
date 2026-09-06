// Guitar lead and guitar chords: Karplus-Strong plucked strings driven by
// your voice (pitch picks the note, level plucks and sustains), through a
// distortion and cabinet lowpass. Also playable from MIDI in.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define KS_LEN 1024

typedef struct {
    float buf[KS_LEN];
    int   wp;
    float len;      // loop length in samples (fractional)
    float prev;
    float decay;
    bool  active;
} ks_t;

static uint32_t seed = 777;

static void ks_pluck(ks_t *s, float freq, float amp)
{
    s->len = FS / freq - 0.5f;               // the averaging filter adds half a sample
    if (s->len < 4) s->len = 4;
    if (s->len > KS_LEN - 4) s->len = KS_LEN - 4;
    int n = (int)s->len + 2;
    float lp = 0;
    for (int i = 0; i < n; i++) {
        float x = frand(&seed);
        lp += 0.5f * (x - lp);               // slightly darker excitation
        s->buf[(s->wp + KS_LEN - n + i) & (KS_LEN - 1)] = lp * amp;
    }
    s->prev = 0;
    s->active = true;
}

static inline float ks_tick(ks_t *s)
{
    if (!s->active) return 0;
    float pos = (float)s->wp - s->len;
    int i0 = (int)floorf(pos);
    float f = pos - i0;
    float a = s->buf[i0 & (KS_LEN - 1)], b = s->buf[(i0 + 1) & (KS_LEN - 1)];
    float y = a + (b - a) * f;
    float fb = 0.5f * (y + s->prev) * s->decay;
    s->prev = y;
    s->buf[s->wp] = fb;
    s->wp = (s->wp + 1) & (KS_LEN - 1);
    return y;
}

// --- shared voice-to-pluck logic --------------------------------------------

#define NSTR 3
static ks_t str[NSTR];
static int  cur_note = -1;
static bool playing;
static float env;               // output envelope
static int  midi_note = -1;     // note held from MIDI in, -1 none
static int  strum_pending, strum_timer;
static biquad_t cab;
static bool chords_mode;
static const int chord_iv[NSTR] = { 0, 7, 12 };  // power chord + octave

static void common_init(bool chords)
{
    chords_mode = chords;
    memset(str, 0, sizeof str);
    for (int i = 0; i < NSTR; i++) str[i].decay = chords ? 0.994f : 0.996f;
    cur_note = -1; playing = false; env = 0; midi_note = -1;
    strum_pending = 0; strum_timer = 0;
    biquad_lowpass(&cab, 2800.0f, 0.8f);
}

static void trigger(int note, float vel)
{
    cur_note = note;
    if (chords_mode) {
        // first string now, the rest staggered by the strum timer
        ks_pluck(&str[0], note_to_freq(note + chord_iv[0]), vel);
        strum_pending = NSTR - 1;
        strum_timer = (int)(0.012f * FS);
    } else {
        ks_pluck(&str[0], note_to_freq(note), vel);
    }
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    (void)in;
    bool gate = v->gate || midi_note >= 0;
    if (midi_note < 0) {
        if (v->voiced && v->note >= 0) {
            float vel = clampf(0.3f + (v->db + 45.0f) / 40.0f, 0.3f, 1.0f);
            if (!playing || v->note != cur_note) { trigger(v->note, vel); playing = true; }
        }
        if (!v->gate) playing = false;
    }
    const float att = onepole_coef(200.0f), rel = onepole_coef(8.0f);
    const float drive = chords_mode ? 6.0f : 12.0f;
    const float mix = chords_mode ? 0.3f : 0.4f;
    for (int i = 0; i < n; i++) {
        if (strum_pending && --strum_timer <= 0) {
            int k = NSTR - strum_pending;
            ks_pluck(&str[k], note_to_freq(cur_note + chord_iv[k]), 0.8f);
            strum_pending--;
            strum_timer = (int)(0.012f * FS);
        }
        float s = 0;
        for (int k = 0; k < NSTR; k++) s += ks_tick(&str[k]);
        onepole(&env, gate ? 1.0f : 0.0f, gate ? att : rel);
        float d = softclip(s * drive * env);
        out[i] = biquad_run(&cab, d) * mix;
    }
}

static void status(char *buf, size_t len)
{
    char nm[5];
    snprintf(buf, len, "%s %s%s", chords_mode ? "chord" : "note",
             note_name(cur_note, nm), midi_note >= 0 ? " (midi)" : "");
}

static void note_on(int note, int vel)
{
    midi_note = note;
    trigger(note, clampf(vel / 127.0f, 0.2f, 1.0f));
    playing = true;
}

static void note_off(int note)
{
    if (note == midi_note) midi_note = -1;
}

static void init_lead(void)   { common_init(false); }
static void init_chords(void) { common_init(true); }

const fx_t fx_guitar_lead   = { "GUITAR LEAD",   init_lead,   process, status, note_on, note_off };
const fx_t fx_guitar_chords = { "GUITAR CHORDS", init_chords, process, status, note_on, note_off };
