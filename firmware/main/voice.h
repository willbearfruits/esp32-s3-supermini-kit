// Voice tracker: level gate plus pitch detection, updated once per audio block.
#pragma once
#include <stdbool.h>

typedef struct {
    float rms;      // block RMS after input gain, linear
    float db;       // same in dBFS
    bool  gate;     // voice present (with hysteresis and hold)
    bool  onset;    // gate opened on this block
    float freq;     // detected pitch in Hz, 0 when none
    float conf;     // 0..1 pitch confidence
    bool  voiced;   // gate && confident pitch
    int   note;     // quantized MIDI note with hysteresis, -1 when none
    float note_f;   // unquantized MIDI note (fractional)
} voice_t;

void voice_init(void);
// Analyse one block of mono samples and update the tracker.
void voice_feed(const float *x, int n, voice_t *out);
