// Channel vocoder: your voice shapes a synth carrier that follows your pitch.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define NB 16
static biquad_t bp_mod[NB], bp_car[NB];
static float env[NB];
static float ph1, ph2, ph_sub;
static float carrier_f = 110.0f, target_f = 110.0f;
static uint32_t seed = 12345;
static float env_c;
static int last_note = -1;

static void init(void)
{
    for (int k = 0; k < NB; k++) {
        float f = 120.0f * powf(5000.0f / 120.0f, (float)k / (NB - 1));
        biquad_bandpass(&bp_mod[k], f, 4.0f);
        biquad_bandpass(&bp_car[k], f, 4.0f);
        env[k] = 0;
    }
    env_c = onepole_coef(40.0f);
    ph1 = ph2 = ph_sub = 0;
    last_note = -1;
}

static inline float saw(float ph) { return 2.0f * ph - 1.0f; }

static void process(const float *in, float *out, int n, const voice_t *v)
{
    if (v->voiced && v->note >= 0) { target_f = note_to_freq(v->note); last_note = v->note; }
    const float f_c = onepole_coef(15.0f);
    for (int i = 0; i < n; i++) {
        onepole(&carrier_f, target_f, f_c);
        float d1 = carrier_f / FS, d2 = carrier_f * 1.006f / FS, ds = carrier_f * 0.5f / FS;
        ph1 += d1; if (ph1 >= 1) ph1 -= 1;
        ph2 += d2; if (ph2 >= 1) ph2 -= 1;
        ph_sub += ds; if (ph_sub >= 1) ph_sub -= 1;
        float car = 0.4f * (saw(ph1) + saw(ph2)) + 0.3f * saw(ph_sub);
        float noise = frand(&seed);
        float x = in[i];
        float y = 0;
        for (int k = 0; k < NB; k++) {
            float m = biquad_run(&bp_mod[k], x);
            float e = onepole(&env[k], fabsf(m), env_c);
            float c = biquad_run(&bp_car[k], k >= NB - 4 ? noise : car);
            y += c * e;
        }
        out[i] = y * 4.0f;
    }
}

static void status(char *buf, size_t len)
{
    char nm[5];
    snprintf(buf, len, "carrier %s %.0fHz", note_name(last_note, nm), carrier_f);
}

const fx_t fx_vocoder = { "VOCODER", init, process, status, NULL, NULL };
