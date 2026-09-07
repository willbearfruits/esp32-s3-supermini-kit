// Stereo mixer and master chain: per-track volume, pan, low cut, tilt tone,
// reverb and ping-pong delay sends, kick sidechain pump, master tone,
// 2x oversampled soft clip, stereo-linked lookahead limiter.
#pragma once
#include <stdbool.h>

#define MIX_MAX 8

enum { FX_NONE, FX_DRIVE, FX_CRUSH, FX_CHORUS, FX_PHASER, FX_WOBBLE, FX_TREMOLO, FX_N };
const char *mix_fx_name(int fx);

typedef struct {
    float vol, pan, rev, dly;   // 0..1, -1..1, 0..1, 0..1
    float lowcut_hz;            // 0 = off
    float tone;                 // -1 dark .. +1 bright
    int   fx; float fx_amt;     // insert effect and its amount 0..1
    bool  mute, duck;           // duck: follows the kick sidechain
} mix_ch_t;

typedef struct {
    float pump;      // 0..1
    float drive;     // 1..4
    float tone;      // -1..1 master tilt
    float rev_mix;   // reverb return
    float dly_fb;    // 0..0.9
} mix_params_t;

void mix_init(void);
void mix_set_tempo(float bpm);
void mix_kick(void);
mix_params_t *mix_params(void);
// tracks[i] holds the dry mono output of track i. Writes interleaved L/R.
void mix_process(float *const tracks[], const mix_ch_t ch[], int nch, float *out_lr, int n);
float mix_gain_reduction(void);
