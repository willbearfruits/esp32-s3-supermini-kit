// Voice looper: build a song layer by layer with nothing but the mic and one
// button. Pages: drums (beatbox), bass, chords, lead, vocal (autotune).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fx.h"

enum { PG_DRUMS, PG_BASS, PG_CHORDS, PG_LEAD, PG_VOCAL, PG_COUNT };
enum { LP_SHORT, LP_LONG, LP_CLEAR };            // button events

#define LOOP_MAX_BARS  8
#define LOOP_MAX_STEPS (LOOP_MAX_BARS * 16)
#define SEQ_ATTACK     0x80                      // seq byte: bit 7 = note starts here

typedef struct {
    int   page;
    bool  armed;             // waiting for the loop to come round
    bool  rec;               // recording on the current page
    float bpm;
    int   bars, steps, step; // loop length in bars / 16ths, current 16th
    int   beats_to_go;       // while armed: beats until recording starts
    bool  has[PG_COUNT];     // layer recorded
    char  key[10];           // "A min" or "free"
    int   live_note;         // what the live voice is playing right now, -1 none
    int   last_drum;         // last classified beatbox hit, -1 none
    float drum_lo, drum_hi;  // its band ratios, for tuning the classifier
    char  msg[24];           // transient message, empty when none
} looper_ui_t;

extern const fx_t fx_looper;

void looper_event(int ev);                       // from the UI task
void looper_get_ui(looper_ui_t *out);
const uint8_t *looper_drum_pattern(void);        // [LOOP_MAX_STEPS][3] velocities
const uint8_t *looper_seq(int page);             // PG_BASS/CHORDS/LEAD, note | SEQ_ATTACK
const char *looper_page_name(int page);
