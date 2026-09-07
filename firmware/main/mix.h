// Mixer and master chain: per-layer volume, mute, reverb and delay sends,
// sidechain pump from the kick, 2x oversampled soft clip, lookahead limiter.
#pragma once
#include <stdbool.h>

#define MIX_N 5

typedef struct { float vol, rev, dly; bool mute; } mix_ch_t;

typedef struct {
    float pump;      // 0..1 how hard the kick ducks everything else
    float drive;     // 1..4 into the clipper
    float rev_mix;   // return level of the reverb
    float dly_fb;    // delay feedback 0..0.9
} mix_params_t;

void mix_init(void);
void mix_set_tempo(float bpm);
void mix_kick(void);                                   // sidechain trigger
mix_params_t *mix_params(void);
// layers[i] holds the dry output of layer i (n samples); channel 0 is the
// drums, which are never ducked. Writes the master into out.
void mix_process(float *const layers[MIX_N], const mix_ch_t ch[MIX_N], float *out, int n);
float mix_gain_reduction(void);                        // limiter GR, 0..1, for the meter
