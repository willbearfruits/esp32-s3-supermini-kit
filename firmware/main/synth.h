// Small subtractive synth: two detuned polyBLEP oscillators, optional sub,
// ADSR amp envelope and a state variable lowpass whose cutoff is recomputed
// every sample from the envelope and velocity.
#pragma once
#include "dsp.h"

#define SYNTH_MAX_VOICES 4

typedef struct {
    float a_ms, d_ms, sus, r_ms;   // amp envelope
    float cutoff, env_hz, vel_hz, q; // lowpass base, envelope sweep, velocity sweep, resonance
    float detune_cents;            // between the two oscillators
    float sub;                     // sub-octave square level
    bool  square;                  // square instead of saw
    float glide_ms;                // portamento (mono use)
    float gain;
} synth_cfg_t;

typedef struct {
    int      note;
    float    vel;
    float    ph1, ph2, phs, freq, freq_t, glide_c;
    float    env;
    int      stage;                // 0 idle 1 attack 2 decay 3 sustain 4 release
    svf_t    lpf;
    uint32_t age;
} synth_voice_t;

typedef struct {
    synth_cfg_t   cfg;
    synth_voice_t v[SYNTH_MAX_VOICES];
    int           nv;
    uint32_t      clock;
} synth_t;

void synth_init(synth_t *s, const synth_cfg_t *cfg, int voices);
void synth_note_on(synth_t *s, int note, float vel);
void synth_note_off(synth_t *s, int note);        // -1: all
void synth_render(synth_t *s, float *out, int n); // adds into out
