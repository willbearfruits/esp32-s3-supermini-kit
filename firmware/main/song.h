// Storage: songs live in slots on a wear-levelled FAT partition
// (/storage/songs/N/state.bin plus one mu-law file per vocal pattern);
// NVS keeps the current slot and the USB-mode flag.
#pragma once
#include <stdbool.h>
#include "wear_levelling.h"

#define STORAGE_BASE "/storage"
#define SONG_SLOTS   8

bool song_init(void);
int  song_slot(void);
bool song_load(int slot);            // apply a slot to the running engine
void song_save_state(void);          // current slot
void song_save_to(int slot);         // state + all vocals, makes it current
void song_save_vocal(void);          // background, only patterns that changed
bool song_saving(void);
void song_export_start(void);
int  song_export_progress(void);     // -1 idle, 0..100
const char *song_export_status(void);
wl_handle_t song_wl(void);
void song_unmount(void);
