// Three synthesised drums plus a beatbox classifier for the mic.
#pragma once
#include <stdbool.h>

enum { DRUM_KICK, DRUM_SNARE, DRUM_HAT, DRUM_N };

void drums_init(void);
void drums_trigger(int type, float vel);
void drums_render(float *out, int n, float gain);   // adds into out

// Band energies of one block, filter state carries across calls.
typedef struct { float lo, hi, all; } drum_bands_t;
void drums_analyse(const float *in, int n, drum_bands_t *b);
// Decide which drum a captured onset sounds like from summed band energies.
int  drums_classify(const drum_bands_t *b, float *lo_ratio, float *hi_ratio);
const char *drums_name(int type);
