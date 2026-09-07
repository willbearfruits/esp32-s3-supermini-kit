// Voice tracker: level gate plus pitch detection, updated once per audio block.
#pragma once
#include <stdbool.h>

typedef struct {
    float rms;         // block RMS after input gain, linear
    float db;          // same in dBFS
    bool  gate;        // voice present (with hysteresis and hold)
    bool  onset;       // gate opened on this block
    int   since_onset; // samples since the gate opened
    float freq;        // pitch in Hz of the held note's median, 0 when none
    float conf;        // 0..1 pitch confidence of the last analysis
    bool  voiced;      // a note is being held
    int   note;        // held MIDI note, -1 when none
    float note_f;      // median unquantised MIDI note (fractional)
} voice_t;

void  voice_init(void);
// Analyse one block of mono samples and update the tracker.
void  voice_feed(const float *x, int n, voice_t *out);
float voice_noise_db(void);   // tracked background level
void  voice_set_gate_db(float db);
float voice_get_gate_db(void);
void  voice_set_stability(int level);   // 0 loose, 1 normal, 2 steady
