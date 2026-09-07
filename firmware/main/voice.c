#include "voice.h"
#include "dsp.h"
#include <string.h>
#include <math.h>
#include "sdkconfig.h"

// Pitch detection runs on a decimated 8 kHz copy of the input, every 8 ms.
#define DEC       (CONFIG_KIT_SAMPLE_RATE / 8000)
#define PFS       8000.0f
#define RING      512                      // decimated samples kept
#define WIN       448                      // analysis window
#define N_INT     320                      // YIN integration length
#define TAU_MIN   8                        // 1000 Hz
#define TAU_MAX   127                      // 63 Hz
#define YIN_THRESH 0.25f
#define HOP_S     0.008f
#define MED_N     5                        // median over the last 5 hops (40 ms)
#define SETTLE    3                        // hops of voice before a note is declared
#define DRIFT     3                        // hops off the note before it moves

static float ring[RING];
static int   rp;
static float dec_acc;
static int   dec_n;

static bool  gate;
static int   hold, hop, since_onset, calib;
static float noise_db = -40;
static int   cur_note = -1, drift, voiced_hops;
static float hist[MED_N];
static int   hist_n;
static float d[TAU_MAX + 1];
static voice_t last;
static float gate_db = CONFIG_KIT_GATE_DB;
static int drift_hops = DRIFT; static float drift_semi = 0.75f;
void voice_set_stability(int level) { drift_hops = level <= 0 ? 2 : (level == 1 ? 3 : 6); drift_semi = level <= 0 ? 0.6f : (level == 1 ? 0.75f : 1.0f); }
void voice_set_gate_db(float db) { gate_db = db; }
float voice_get_gate_db(void) { return gate_db; }

void voice_init(void)
{
    memset(ring, 0, sizeof ring);
    memset(&last, 0, sizeof last);
    rp = 0; dec_acc = 0; dec_n = 0; hop = 0;
    gate = false; hold = 0; cur_note = -1; drift = 0; voiced_hops = 0; hist_n = 0;
    noise_db = -40; calib = (int)(0.7f * FS);   // samples of start-up used to learn the room
    last.note = -1;
}

float voice_noise_db(void) { return noise_db; }

static float yin(const float *w, float *conf_out)
{
    for (int tau = TAU_MIN; tau <= TAU_MAX; tau++) {
        float s = 0;
        for (int j = 0; j < N_INT; j++) {
            float diff = w[j] - w[j + tau];
            s += diff * diff;
        }
        d[tau] = s;
    }
    float run = 0, best = 1e9f;
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
    int tau = best_tau;
    for (int t = TAU_MIN; t <= TAU_MAX; t++) {
        if (d[t] < YIN_THRESH) {
            tau = t;
            while (tau + 1 <= TAU_MAX && d[tau + 1] < d[tau]) tau++;
            break;
        }
    }
    float cm = d[tau];
    float tf = tau;
    if (tau > TAU_MIN && tau < TAU_MAX) {
        float a = d[tau - 1], b = d[tau], c = d[tau + 1];
        float den = a - 2 * b + c;
        if (fabsf(den) > 1e-9f) tf = tau + 0.5f * (a - c) / den;
    }
    *conf_out = clampf(1.0f - cm, 0, 1);
    return PFS / tf;
}

static float median5(void)
{
    float t[MED_N];
    int n = hist_n < MED_N ? hist_n : MED_N;
    memcpy(t, hist, n * sizeof(float));
    for (int i = 1; i < n; i++) { float v = t[i]; int j = i; while (j > 0 && t[j - 1] > v) { t[j] = t[j - 1]; j--; } t[j] = v; }
    return t[n / 2];
}

void voice_feed(const float *x, int n, voice_t *v)
{
    float acc = 0;
    for (int i = 0; i < n; i++) acc += x[i] * x[i];
    float rms = sqrtf(acc / n);
    float db = rms > 1e-9f ? 20 * log10f(rms) : -180;

    // background: learned quickly at start-up, then drops fast, rises slowly
    const float blk = (float)n / FS;
    if (calib > 0) { calib -= n; noise_db += 0.3f * (db - noise_db); }
    else if (db < noise_db) noise_db += (1 - expf(-blk / 0.3f)) * (db - noise_db);
    else noise_db += (1 - expf(-blk / 20.0f)) * (db - noise_db);
    if (noise_db < -90) noise_db = -90;

    float open_db = gate_db;
    if (noise_db + 15 > open_db) open_db = noise_db + 15;
    const float close_db = open_db - 8;
    const int hold_blocks = (int)(0.12f * FS / n);
    bool was = gate;
    if (!gate && db > open_db) { gate = true; hold = hold_blocks; since_onset = 0; }
    else if (gate) {
        if (db > close_db) hold = hold_blocks;
        else if (--hold <= 0) gate = false;
    }
    if (gate) since_onset += n;

    for (int i = 0; i < n; i++) {
        dec_acc += x[i];
        if (++dec_n == DEC) {
            ring[rp] = dec_acc / DEC;
            rp = (rp + 1) & (RING - 1);
            dec_acc = 0; dec_n = 0;
        }
    }

    if (!gate) {
        cur_note = -1; drift = 0; voiced_hops = 0; hist_n = 0;
        last.freq = 0; last.conf = 0; last.note_f = 0;
    }

    hop += n;
    if (gate && hop >= (int)(HOP_S * FS)) {
        hop = 0;
        float w[WIN];
        int start = (rp - WIN) & (RING - 1);
        for (int i = 0; i < WIN; i++) w[i] = ring[(start + i) & (RING - 1)];
        float conf, freq = yin(w, &conf);
        bool ok = conf > 0.65f && freq > 60 && freq < 1100;
        last.conf = conf;
        if (ok) {
            float nf = freq_to_note(freq);
            if (hist_n < MED_N) hist[hist_n++] = nf;
            else { memmove(hist, hist + 1, (MED_N - 1) * sizeof(float)); hist[MED_N - 1] = nf; }
            float med = median5();
            voiced_hops++;
            last.note_f = med;
            if (voiced_hops >= SETTLE) {
                int near = (int)lroundf(med);
                if (cur_note < 0) { cur_note = near; drift = 0; }
                else if (fabsf(med - cur_note) > drift_semi) { if (++drift >= drift_hops) { cur_note = near; drift = 0; } }
                else drift = 0;
            }
        } else {
            voiced_hops = 0;
        }
    }

    last.rms = rms; last.db = db; last.gate = gate; last.onset = gate && !was;
    last.since_onset = gate ? since_onset : 0;
    last.voiced = cur_note >= 0;
    last.note = cur_note;
    last.freq = last.voiced ? note_to_freq(last.note_f) : 0;
    *v = last;
}
