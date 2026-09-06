// Hyperpop autotune: hard, instant snap of the voice to the scale, with an
// octave-up double and some drive.
#pragma once
#include "voice.h"
#include "scale.h"

void tune_init(void);
void tune_process(const float *in, float *out, int n, const voice_t *v, const scale_t *sc);
int  tune_target(void);   // MIDI note currently tuned to, -1 when none
