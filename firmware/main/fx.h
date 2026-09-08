// Effect / instrument interface. One mode is active at a time; the audio
// engine calls process() once per block with mono float input and output.
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "voice.h"

typedef struct {
    const char *name;
    void (*init)(void);                                                  // reset state
    void (*process)(const float *in, float *out, int n, const voice_t *v);
    void (*status)(char *buf, size_t len);                               // one OLED line
    void (*note_on)(int note, int vel);                                  // optional, MIDI in
    void (*note_off)(int note);                                          // optional, MIDI in
    bool stereo;                                                         // process() writes interleaved L/R (2n floats)
} fx_t;

extern const fx_t fx_vocoder, fx_autotune, fx_guitar_lead, fx_guitar_chords,
                  fx_delay, fx_reverb, fx_stutter, fx_test, fx_sine;

extern const fx_t *const fx_list[];
extern const int fx_count;
