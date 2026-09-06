// Hard autotune: pitch-shift the voice to the nearest semitone.
// Delay-line shifter with two crossfaded taps; corrections are small so it
// stays clean.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define L   1024                 // delay line (power of two)
#define W   512.0f               // modulation window in samples
static float dl[L];
static int   wp;
static float ph;                 // 0..1 phasor
static float ratio = 1.0f, wet = 0;
static float cents;

static void init(void)
{
    memset(dl, 0, sizeof dl);
    wp = 0; ph = 0; ratio = 1; wet = 0; cents = 0;
}

static inline float tap(float delay)
{
    float pos = (float)wp - delay;
    int i0 = (int)floorf(pos);
    float f = pos - i0;
    float a = dl[i0 & (L - 1)], b = dl[(i0 + 1) & (L - 1)];
    return a + (b - a) * f;
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    float target = 1.0f;
    if (v->voiced && v->note >= 0 && v->freq > 0) {
        target = note_to_freq(v->note) / v->freq;
        target = clampf(target, 0.7f, 1.4f);
        cents = (v->note - v->note_f) * 100.0f;
    }
    const float rc = onepole_coef(60.0f), wc = onepole_coef(20.0f);
    for (int i = 0; i < n; i++) {
        onepole(&ratio, target, rc);
        onepole(&wet, v->voiced ? 1.0f : 0.0f, wc);
        dl[wp] = in[i];
        ph += (1.0f - ratio) / W;
        if (ph >= 1) ph -= 1;
        if (ph < 0) ph += 1;
        float ph2 = ph + 0.5f;
        if (ph2 >= 1) ph2 -= 1;
        float g1 = sinf(3.14159265f * ph), g2 = sinf(3.14159265f * ph2);
        float y = tap(ph * W) * g1 + tap(ph2 * W) * g2;
        out[i] = wet * y + (1.0f - wet) * in[i];
        wp = (wp + 1) & (L - 1);
    }
}

static void status(char *buf, size_t len)
{
    snprintf(buf, len, "shift %+.0f cents", wet > 0.5f ? cents : 0.0f);
}

const fx_t fx_autotune = { "AUTOTUNE", init, process, status, NULL, NULL };
