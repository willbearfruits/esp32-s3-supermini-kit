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

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float softclip(float x) { return tanhf(x); }

// --- notes
static inline float note_to_freq(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }
static inline float freq_to_note(float f) { return 69.0f + 12.0f * log2f(f / 440.0f); }
// Writes e.g. "A#3" into buf (needs 5 bytes).
const char *note_name(int midi, char *buf);
