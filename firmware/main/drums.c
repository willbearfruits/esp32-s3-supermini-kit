#include "drums.h"
#include "dsp.h"
#include <string.h>
#include "esp_attr.h"

typedef struct { bool on; float vel, amp, amp2, penv, ph, ph2; } dv_t;
static dv_t d[DRUM_N];
static float k_amp_c, k_pitch_c, s_amp_c, s_tone_c, h_amp_c;   // per-sample decay multipliers
static svf_t snare_bp, hat_hp;
static biquad_t an_lo, an_hi;
static uint32_t seed = 12345;

static float decay(float tau_s) { return expf(-1.0f / (tau_s * FS)); }

void drums_init(void)
{
    memset(d, 0, sizeof d);
    k_amp_c = decay(0.16f); k_pitch_c = decay(0.035f);
    s_amp_c = decay(0.075f); s_tone_c = decay(0.035f);
    h_amp_c = decay(0.022f);
    svf_set(&snare_bp, 1800.0f, 1.2f); svf_reset(&snare_bp);
    svf_set(&hat_hp, 7000.0f, 0.7f);   svf_reset(&hat_hp);
    biquad_lowpass(&an_lo, 250.0f, 0.707f);
    biquad_highpass(&an_hi, 2500.0f, 0.707f);
}

void drums_trigger(int type, float vel)
{
    if (type < 0 || type >= DRUM_N) return;
    dv_t *v = &d[type];
    v->on = true; v->vel = vel; v->amp = 1; v->amp2 = 1; v->penv = 1; v->ph = 0; v->ph2 = 0;
}

void IRAM_ATTR drums_render(float *out, int n, float gain)
{
    const float inv_fs = 1.0f / FS;
    dv_t *k = &d[DRUM_KICK], *s = &d[DRUM_SNARE], *h = &d[DRUM_HAT];
    for (int i = 0; i < n; i++) {
        float y = 0;
        float noise = frand(&seed);
        if (k->on) {
            float f = 48.0f + 130.0f * k->penv;
            k->ph += f * inv_fs; if (k->ph >= 1) k->ph -= 1;
            y += (fast_sin01(k->ph) * k->amp + (k->penv > 0.9f ? 0.4f * noise : 0.0f)) * k->vel * 1.2f;
            k->amp *= k_amp_c; k->penv *= k_pitch_c;
            if (k->amp < 0.002f) k->on = false;
        }
        float bp;
        svf_run(&snare_bp, noise, &bp, NULL);
        if (s->on) {
            s->ph += 190.0f * inv_fs; if (s->ph >= 1) s->ph -= 1;
            y += (bp * s->amp * 1.6f + fast_sin01(s->ph) * s->amp2 * 0.6f) * s->vel;
            s->amp *= s_amp_c; s->amp2 *= s_tone_c;
            if (s->amp < 0.002f) s->on = false;
        }
        float hp;
        svf_run(&hat_hp, noise, NULL, &hp);
        if (h->on) {
            y += hp * h->amp * h->vel * 1.1f;
            h->amp *= h_amp_c;
            if (h->amp < 0.002f) h->on = false;
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
