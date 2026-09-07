// Drum kits: three synthesised kits plus a sample kit loaded from storage,
// and the beatbox classifier for the mic. Instances, so export can render
// a second copy.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "dsp.h"

enum { DRUM_KICK, DRUM_SNARE, DRUM_HAT, DRUM_N };
#define KIT_SYNTH_N 3
#define KIT_USER    KIT_SYNTH_N         // index of the sample kit
#define KIT_N       (KIT_SYNTH_N + 1)

typedef struct { int16_t *data; int len; float step; } kit_sample_t;   // step = rate / FS

typedef struct {
    bool  on;
    float vel, amp, amp2, penv, ph, ph2, pos;
} drum_voice_t;

typedef struct {
    int   kit;
    drum_voice_t v[DRUM_N];
    float k_amp_c, k_pitch_c, s_amp_c, s_tone_c, h_amp_c;
    svf_t snare_bp, hat_hp;
    uint32_t seed;
} drums_t;

void  drums_init(drums_t *d, int kit);
void  drums_set_kit(drums_t *d, int kit);
void  drums_trigger(drums_t *d, int type, float vel);
void  drums_render(drums_t *d, float *out, int n, float gain);   // adds into out
const char *kit_name(int kit);
bool  kit_user_loaded(void);
// Load kick.wav / snare.wav / hat.wav from dir into PSRAM (16-bit PCM, any rate)
int   kit_load_user(const char *dir);

// Beatbox classifier (shared, runs on the mic)
typedef struct { float lo, hi, all; } drum_bands_t;
void  drums_analyser_init(void);
void  drums_analyse(const float *in, int n, drum_bands_t *b);
int   drums_classify(const drum_bands_t *b, float *lo_ratio, float *hi_ratio);
const char *drums_name(int type);
