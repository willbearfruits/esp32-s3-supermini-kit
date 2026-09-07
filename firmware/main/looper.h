// Voice looper engine. Five layers on a fixed grid: drums (beatbox), bass,
// chords, lead (hummed or whistled), vocal (hyperpop autotune). One take is
// one loop, counted in by the metronome. Everything the UI needs goes
// through this header; the engine runs inside the audio task.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fx.h"
#include "kit.h"

enum { PG_DRUMS, PG_BASS, PG_CHORDS, PG_LEAD, PG_VOCAL, PG_COUNT };

#define LOOP_MAX_BARS  8
#define LOOP_MAX_STEPS (LOOP_MAX_BARS * 16)
#define SEQ_ATTACK     0x80

// actions, from the UI task
enum { ACT_TAP, ACT_CANCEL, ACT_NEXT_PAGE, ACT_PREV_PAGE, ACT_UNDO, ACT_CLEAR_LAYER,
       ACT_MUTE, ACT_CLEAR_SONG, ACT_REPLACE };
// parameters, integer valued
enum { PR_VOL, PR_REV, PR_DLY, PR_SWING, PR_HUMAN, PR_PUMP, PR_DRIVE, PR_KIT, PR_BPM, PR_BARS, PR_N };

typedef struct {
    int   page;
    bool  armed, rec, replace;
    float bpm;
    int   bars, steps, step, beat;
    int   beats_to_go;
    bool  has[PG_COUNT], mute[PG_COUNT];
    char  key[10];
    int   live_note, last_drum;
    float drum_lo, drum_hi;
    char  msg[24];
    float level;              // master peak 0..1 since last call
    float gr;                 // limiter gain reduction 0..1
    int   bounce;             // 0 idle, else pass in progress
    int   undo_page;          // page an undo would restore, -1 none
} looper_ui_t;

// persistent song state, saved as one blob
typedef struct {
    uint32_t magic;
    uint16_t bpm10;
    uint8_t  bars, page, swing, human, pump, drive10, kit;
    uint8_t  root, minor, scale_locked;
    struct { uint8_t has, mute, vol, rev, dly; } layer[PG_COUNT];
    uint8_t  drum_pat[LOOP_MAX_STEPS][DRUM_N];
    uint8_t  seq[3][LOOP_MAX_STEPS];
} song_state_t;
#define SONG_MAGIC 0x4B495431   // "KIT1"

enum { DIRTY_STATE = 1, DIRTY_VOCAL = 2 };

extern const fx_t fx_looper;

void looper_action(int act);
int  looper_param_get(int p);
void looper_param_set(int p, int v);
void looper_param_step(int p, int dir);
const char *looper_param_name(int p);
void looper_param_text(int p, char *buf, int len);
void looper_expr(float cut, float bend, float vib, float space);   // cutoff multiplier, semitones, 0..1, 0..1
void looper_midi_note(int note, int vel, bool on);
void looper_get_ui(looper_ui_t *out);
const uint8_t *looper_drum_pattern(void);
const uint8_t *looper_seq(int page);
const char *looper_page_name(int page);

// persistence (UI task)
int  looper_take_dirty(void);                       // returns and clears DIRTY_* bits
void looper_get_state(song_state_t *st);
void looper_set_state(const song_state_t *st);      // before or after audio start
uint8_t *looper_vocal_buf(void);                    // mu-law, loop_len bytes
int  looper_loop_len(void);
void looper_vocal_loaded(void);                     // vocal buffer filled from storage
// clock for MIDI out: called from the audio task, 24 per beat
void looper_set_clock_cb(void (*cb)(void));

// export by bouncing in real time: pass 1 captures drums, bass, chords;
// pass 2 captures lead, vocal and the master. Buffers are int16, loop_len each.
bool looper_bounce_start(int pass);
bool looper_bounce_done(void);
int16_t *looper_bounce_buf(int i);
void looper_bounce_release(void);
