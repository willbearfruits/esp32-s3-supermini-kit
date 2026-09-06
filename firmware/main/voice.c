#include "voice.h"
#include "dsp.h"
#include <string.h>
#include <math.h>
#include "sdkconfig.h"

// Pitch detection runs on a 2x decimated copy of the input to keep YIN cheap.
#define DEC       2
#define PFS       (FS / DEC)
#define RING      512                      // decimated samples kept
#define WIN       448                      // analysis window
#define N_INT     320                      // YIN integration length
#define TAU_MIN   8                        // 1000 Hz at 8 kHz
#define TAU_MAX   127                      // 63 Hz at 8 kHz
#define YIN_THRESH 0.25f

static float ring[RING];
static int   rp;
static float dec_acc;
static int   dec_n;

static bool  gate;
static int   hold;          // blocks left before the gate may close
static float noise_db = -40; // tracked background level
static int   cur_note = -1;
static int   cand_note = -1, cand_count;
static float d[TAU_MAX + 1];

void voice_init(void)
{
    memset(ring, 0, sizeof ring);
    rp = 0; dec_acc = 0; dec_n = 0;
    gate = false; hold = 0; cur_note = -1; cand_note = -1; cand_count = 0;
    noise_db = -40;
}

static float yin(const float *w, float *conf_out)
{
    // difference function
    for (int tau = TAU_MIN; tau <= TAU_MAX; tau++) {
        float s = 0;
        for (int j = 0; j < N_INT; j++) {
            float diff = w[j] - w[j + tau];
            s += diff * diff;
        }
        d[tau] = s;
    }
    // cumulative mean normalised difference (taus below TAU_MIN count as d[TAU_MIN])
    float run = 0;
    float best = 1e9f;
    int best_tau = -1;
    for (int tau = 1; tau <= TAU_MAX; tau++) {
        float raw = tau < TAU_MIN ? d[TAU_MIN] : d[tau];
        run += raw;
        if (tau >= TAU_MIN) {
            float cm = run > 0 ? raw * tau / run : 1.0f;
            d[tau] = cm;
            if (cm < best) { best = cm; best_tau = tau; }
        }
    }
    if (best_tau < 0) { *conf_out = 0; return 0; }
    // Prefer the first dip below threshold over the global minimum (avoids octave errors)
    int tau = best_tau;
    for (int t = TAU_MIN; t <= TAU_MAX; t++) {
        if (d[t] < YIN_THRESH) {
            tau = t;
            while (tau + 1 <= TAU_MAX && d[tau + 1] < d[tau]) tau++;
            break;
        }
    }
    float cm = d[tau];
    // parabolic interpolation
    float tf = tau;
    if (tau > TAU_MIN && tau < TAU_MAX) {
        float a = d[tau - 1], b = d[tau], c = d[tau + 1];
        float den = a - 2 * b + c;
        if (fabsf(den) > 1e-9f) tf = tau + 0.5f * (a - c) / den;
    }
    *conf_out = clampf(1.0f - cm, 0, 1);
    return PFS / tf;
}

void voice_feed(const float *x, int n, voice_t *v)
{
    // level
    float acc = 0;
    for (int i = 0; i < n; i++) acc += x[i] * x[i];
    float rms = sqrtf(acc / n);
    float db = rms > 1e-9f ? 20 * log10f(rms) : -180;

    // background level: drops fast, rises very slowly (about 20 s)
    if (db < noise_db) noise_db += 0.2f * (db - noise_db);
    else noise_db += 0.002f * (db - noise_db);
    if (noise_db < -90) noise_db = -90;

    // gate with hysteresis and a hold time; must clear both the fixed
    // threshold and the background by a margin
    float open_db = CONFIG_KIT_GATE_DB;
    if (noise_db + 10 > open_db) open_db = noise_db + 10;
    const float close_db = open_db - 8;
    const int hold_blocks = (int)(0.12f * FS / n);
    bool was = gate;
    if (!gate && db > open_db) { gate = true; hold = hold_blocks; }
    else if (gate) {
        if (db > close_db) hold = hold_blocks;
        else if (--hold <= 0) gate = false;
    }

    // decimate into the ring
    for (int i = 0; i < n; i++) {
        dec_acc += x[i];
        if (++dec_n == DEC) {
            ring[rp] = dec_acc / DEC;
            rp = (rp + 1) & (RING - 1);
            dec_acc = 0; dec_n = 0;
        }
    }

    float freq = 0, conf = 0;
    if (gate) {
        float w[WIN];
        int start = (rp - WIN) & (RING - 1);
        for (int i = 0; i < WIN; i++) w[i] = ring[(start + i) & (RING - 1)];
        freq = yin(w, &conf);
    }
    bool voiced = gate && conf > 0.6f && freq > 60 && freq < 1100;

    // note quantisation with hysteresis: only move when clearly on a new note
    float note_f = voiced ? freq_to_note(freq) : 0;
    if (voiced) {
        int nearest = (int)lroundf(note_f);
        if (cur_note < 0) { cur_note = nearest; cand_note = nearest; cand_count = 0; }
        else if (fabsf(note_f - cur_note) > 0.6f) {
            if (nearest == cand_note) { if (++cand_count >= 2) cur_note = nearest; }
            else { cand_note = nearest; cand_count = 0; }
        }
    } else if (!gate) {
        cur_note = -1; cand_note = -1; cand_count = 0;
    }

    v->rms = rms; v->db = db; v->gate = gate; v->onset = gate && !was;
    v->freq = freq; v->conf = conf; v->voiced = voiced;
    v->note = voiced ? cur_note : (gate ? cur_note : -1);
    v->note_f = note_f;
}
