#include "drums.h"
#include "dsp.h"
#include <string.h>

typedef struct { bool on; float t, vel, ph, ph2; } dv_t;
static dv_t d[DRUM_N];
static biquad_t snare_bp, hat_hp, an_lo, an_hi;
static uint32_t seed = 12345;

void drums_init(void)
{
    memset(d, 0, sizeof d);
    biquad_bandpass(&snare_bp, 1800.0f, 0.8f);
    biquad_highpass(&hat_hp, 6500.0f, 0.7f);
    biquad_lowpass(&an_lo, 250.0f, 0.707f);
    biquad_highpass(&an_hi, 2500.0f, 0.707f);
}

void drums_trigger(int type, float vel)
{
    if (type < 0 || type >= DRUM_N) return;
    d[type].on = true; d[type].t = 0; d[type].vel = vel; d[type].ph = 0; d[type].ph2 = 0;
}

void drums_render(float *out, int n, float gain)
{
    const float dt = 1.0f / FS;
    for (int i = 0; i < n; i++) {
        float y = 0;
        dv_t *k = &d[DRUM_KICK];
        if (k->on) {
            float f = 48.0f + 120.0f * expf(-k->t * 30.0f);
            k->ph += f * dt; if (k->ph >= 1) k->ph -= 1;
            float a = expf(-k->t * 6.0f);
            y += (sinf(TWO_PI * k->ph) * a + (k->t < 0.002f ? 0.5f : 0.0f) * frand(&seed)) * k->vel * 1.2f;
            k->t += dt; if (a < 0.002f) k->on = false;
        }
        dv_t *s = &d[DRUM_SNARE];
        float sn = frand(&seed);
        float bp = biquad_run(&snare_bp, sn);
        if (s->on) {
            s->ph += 190.0f * dt; if (s->ph >= 1) s->ph -= 1;
            float a = expf(-s->t * 13.0f), b = expf(-s->t * 28.0f);
            y += (bp * a * 1.4f + sinf(TWO_PI * s->ph) * b * 0.6f) * s->vel;
            s->t += dt; if (a < 0.002f) s->on = false;
        }
        dv_t *h = &d[DRUM_HAT];
        float hp = biquad_run(&hat_hp, sn);
        if (h->on) {
            float a = expf(-h->t * 45.0f);
            y += hp * a * h->vel * 1.1f;
            h->t += dt; if (a < 0.002f) h->on = false;
        }
        out[i] += y * gain;
    }
}

void drums_analyse(const float *in, int n, drum_bands_t *b)
{
    float lo = 0, hi = 0, all = 0;
    for (int i = 0; i < n; i++) {
        float l = biquad_run(&an_lo, in[i]), h = biquad_run(&an_hi, in[i]);
        lo += l * l; hi += h * h; all += in[i] * in[i];
    }
    b->lo = lo; b->hi = hi; b->all = all;
}

int drums_classify(const drum_bands_t *b, float *lo_ratio, float *hi_ratio)
{
    float all = b->all > 1e-12f ? b->all : 1e-12f;
    float lr = b->lo / all, hr = b->hi / all;
    if (lo_ratio) *lo_ratio = lr;
    if (hi_ratio) *hi_ratio = hr;
    if (hr > 0.35f) return DRUM_HAT;
    if (lr > 0.40f) return DRUM_KICK;
    return DRUM_SNARE;
}

const char *drums_name(int type)
{
    static const char *n[DRUM_N] = { "KICK", "SNARE", "HAT" };
    return type >= 0 && type < DRUM_N ? n[type] : "?";
}
