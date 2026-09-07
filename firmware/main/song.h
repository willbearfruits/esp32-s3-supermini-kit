// Storage: NVS for the song state, a wear-levelled FAT partition for the
// vocal loop, user drum kits and exports.
#pragma once
#include <stdbool.h>
#include "wear_levelling.h"

#define STORAGE_BASE "/storage"

bool song_init(void);                 // nvs + mount; false if storage unusable
bool song_load(void);                 // apply saved state to the running engine
void song_save_state(void);
void song_save_vocal(void);           // background, a few seconds
bool song_saving(void);
void song_export_start(void);         // background bounce + WAV/MIDI writing
int  song_export_progress(void);      // -1 idle, 0..100 running, 100 done
const char *song_export_status(void);
wl_handle_t song_wl(void);
void song_unmount(void);
