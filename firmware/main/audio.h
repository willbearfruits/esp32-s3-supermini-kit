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
void audio_midi_note_on(int note, int vel);
void audio_midi_note_off(int note);
