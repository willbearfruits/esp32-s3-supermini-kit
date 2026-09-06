// USB composite device: MIDI (voice-to-MIDI out, MIDI in to the instrument)
// plus a CDC serial port that carries the console.
#pragma once
#include <stdbool.h>
#include "voice.h"

void usb_midi_init(void);
bool usb_midi_connected(void);
void usb_midi_track_voice(const voice_t *v);   // call once per audio block
void usb_midi_program_change(int program);
