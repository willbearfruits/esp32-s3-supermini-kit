#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "voice.h"

#define AUDIO_WAVE_N 128

void audio_start(void);
void audio_set_mode(int idx);          // switches at the next block boundary
int  audio_get_mode(void);
void audio_get_voice(voice_t *out);
void audio_get_wave(int8_t *out);      // last output block, 1 point per column
float audio_out_peak(void);            // output peak since last call, linear
float audio_cpu_load(void);            // DSP time / available time, 0..1
void  audio_set_mic_gain(float g);     // linear, runtime
float audio_get_mic_gain(void);
void  audio_restart_i2s(void);
int   audio_pin_held(int pin);         // 0 free, 1 held low, 2 held high by the wiring
void  audio_get_raw_stats(int *mn, int *mx, int *dc);   // 24-bit sample min/max/mean of the last block
void  audio_get_raw(int32_t *l, int32_t *r, int *nz_l, int *nz_r);   // last raw I2S words and non-zero counts per block
void  audio_get_raw_live(int *live_l, int *live_r);   // words per block whose top 24 bits are neither 0 nor -1
int   audio_mic_slot(void);           // 0 = mic found in the left slot, 1 = right
void audio_midi_note_on(int note, int vel);
void audio_midi_note_off(int note);
