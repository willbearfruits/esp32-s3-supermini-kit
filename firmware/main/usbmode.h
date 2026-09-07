// USB mode: the storage partition as a USB drive plus a class-compliant
// MIDI device (voice to MIDI out, MIDI clock, MIDI in to the synths).
// Entered by a menu item or by holding the encoder at power-up; the
// console and normal flashing are unavailable while it runs.
#pragma once
#include <stdbool.h>
#include "voice.h"
#include "wear_levelling.h"

bool usbmode_requested(void);        // NVS flag set by the menu
void usbmode_request(bool on);
void usbmode_start(wl_handle_t wl);
bool usbmode_active(void);
bool usbmode_host_connected(void);
void usbmode_track_voice(const voice_t *v);   // from the audio task
