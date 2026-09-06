// Small DSP toolbox shared by the effects. Everything is float, mono.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#define FS ((float)CONFIG_KIT_SAMPLE_RATE)
#define TWO_PI 6.28318530717958647692f

// --- biquad (RBJ cookbook), direct form 2 transposed
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } biquad_t;

void biquad_lowpass(biquad_t *q, float f0, float Q);
void biquad_highpass(biquad_t *q, float f0, float Q);
void biquad_bandpass(biquad_t *q, float f0, float Q);
static inline void biquad_reset(biquad_t *q) { q->z1 = q->z2 = 0; }

static inline float biquad_run(biquad_t *q, float x)
{
    float y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}

// --- one-pole smoother: state follows x with coefficient c (0..1)
static inline float onepole(float *s, float x, float c) { *s += c * (x - *s); return *s; }
// coefficient for a given cutoff in Hz
static inline float onepole_coef(float hz) { return 1.0f - expf(-TWO_PI * hz / FS); }

// --- cheap white noise in -1..1
static inline float frand(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (float)((*seed >> 9) & 0x7FFF) / 16384.0f - 1.0f;
}

// --- fast maths for per-sample control paths (no libm in the hot loops)
// tan(x) for 0 <= x < ~0.8, i.e. cutoffs below a quarter of the sample rate
static inline float fast_tan(float x)
{
    float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * (0.133333f + x2 * 0.053968f)));
}
// sin(2*pi*p) for a phase p in 0..1, about 0.1 % error, good for drums/LFOs
static inline float fast_sin01(float p)
{
    float y = p < 0.5f ? 16.0f * p * (0.5f - p) : -16.0f * (p - 0.5f) * (1.0f - p);
    return y * (0.775f + 0.225f * fabsf(y));
}

// --- TPT state variable filter (Zavalishin). svf_set is cheap enough to
// call every sample, which is what makes filter sweeps click-free.
typedef struct { float ic1, ic2, k, a1, a2, a3; } svf_t;
static inline void svf_set(svf_t *f, float hz, float q)
{
    float g = fast_tan(3.14159265f * hz / FS);
    f->k = 1.0f / q;
    f->a1 = 1.0f / (1.0f + g * (g + f->k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}
static inline void svf_reset(svf_t *f) { f->ic1 = f->ic2 = 0; }
// returns lowpass; *bp / *hp optional
static inline float svf_run(svf_t *f, float x, float *bp, float *hp)
{
    float v3 = x - f->ic2;
    float v1 = f->a1 * f->ic1 + f->a2 * v3;
    float v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    if (bp) *bp = v1;
    if (hp) *hp = x - f->k * v1 - v2;
    return v2;
}
#define SVF_MAX_HZ (FS * 0.22f)

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float softclip(float x) { return tanhf(x); }

// --- notes
static inline float note_to_freq(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }
static inline float freq_to_note(float f) { return 69.0f + 12.0f * log2f(f / 440.0f); }
// Writes e.g. "A#3" into buf (needs 5 bytes).
const char *note_name(int midi, char *buf);
