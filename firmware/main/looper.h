// Voice looper engine: up to 8 tracks of five kinds on a fixed grid, four
// patterns per track selected by scene, an arrangement of scenes, stereo
// mix. One take is one loop, counted in by the metronome. The engine runs
// inside the audio task; the UI talks to it through this header.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fx.h"
#include "kit.h"

#define TRACK_N        8
#define PAT_N          4
#define LOOP_MAX_BARS  8
#define LOOP_MAX_STEPS (LOOP_MAX_BARS * 16)
#define SEQ_ATTACK     0x80
#define VOCAL_SLOTS    4
#define ARR_N          8

enum { K_DRUMS, K_BASS, K_KEYS, K_LEAD, K_VOCAL, K_N };

enum { ACT_TAP, ACT_CANCEL, ACT_NEXT_TRACK, ACT_PREV_TRACK, ACT_UNDO, ACT_CLEAR_PAT, ACT_MUTE, ACT_SOLO,
       ACT_CLEAR_SONG, ACT_REPLACE, ACT_SCENE_UP, ACT_SCENE_DOWN, ACT_ADD_TRACK, ACT_DEL_TRACK,
       ACT_COPY_A, ACT_SONG_MODE, ACT_NEW_SONG };

enum { PR_VOL, PR_PAN, PR_REV, PR_DLY, PR_LOWCUT, PR_TONE, PR_SOUND,
       PR_KIT, PR_SWING, PR_HUMAN, PR_BPM, PR_BARS, PR_KEY, PR_COUNTIN, PR_METRO, PR_QUANT, PR_MICGAIN, PR_GATE, PR_ADDKIND,
       PR_PUMP, PR_DRIVE, PR_MTONE,
       PR_SCENE, PR_ARR0, PR_ARR1, PR_ARR2, PR_ARR3, PR_ARR4, PR_ARR5, PR_ARR6, PR_ARR7, PR_ARR_REP,
       PR_N };

typedef struct {
    int   track, n_tracks, kind;
    char  track_name[16], sound[12];
    int   scene, scene_next;          // current, queued (-1 none)
    bool  song_mode; int arr_pos;
    bool  armed, rec, replace;
    float bpm;
    int   bars, steps, step, beat, beats_to_go;
    struct { uint8_t kind; bool has, mute, solo; } tr[TRACK_N];
    char  key[10];
    int   live_note, last_drum;
    float drum_lo, drum_hi;
    char  msg[24];
    float level, gr;
    int   bounce;
    bool  can_undo;
} looper_ui_t;

// persistent song, saved as a file
typedef struct {
    uint32_t magic;
    uint16_t bpm10;
    uint8_t  bars, cur_track, scene, count_in, metro, quant8, swing, human, pump, drive10, kit, key, mic_gain, n_tracks;
    int8_t   mtone, gate_db;
    uint8_t  root, minor, scale_locked, song_mode, arr_rep;
    uint8_t  arr[ARR_N];
    struct { uint8_t kind, sound, mute, solo, vol, rev, dly, lowcut; int8_t pan, tone; uint8_t has[PAT_N]; } tr[TRACK_N];
    uint8_t  drum[TRACK_N][PAT_N][LOOP_MAX_STEPS][DRUM_N];
    uint8_t  seq[TRACK_N][PAT_N][LOOP_MAX_STEPS];
} song_state_t;
#define SONG_MAGIC 0x4B495432   // "KIT2"

enum { DIRTY_STATE = 1, DIRTY_VOCAL = 2 };

extern const fx_t fx_looper;

void looper_action(int act);
int  looper_param_get(int p);
void looper_param_set(int p, int v);
void looper_param_step(int p, int dir);
const char *looper_param_name(int p);
void looper_param_text(int p, char *buf, int len);
void looper_expr(float cut, float bend, float vib, float space);
void looper_midi_note(int note, int vel, bool on);
void looper_get_ui(looper_ui_t *out);
const uint8_t *looper_drum_pattern(void);     // current track, pattern of the current scene
const uint8_t *looper_seq(void);
const char *looper_kind_name(int kind);
bool looper_ready(void);

int  looper_take_dirty(void);
void looper_get_state(song_state_t *st);
void looper_set_state(const song_state_t *st);
// vocal audio: slot buffers, mu-law, loop_len bytes. dirty_vocal reports which (track,pat)
int  looper_loop_len(void);
int  looper_vocal_slot(int track, int pat);   // -1 none
uint8_t *looper_vocal_data(int slot);
int  looper_vocal_alloc(int track, int pat);  // for loading; -1 when full
void looper_vocal_loaded(int track, int pat);
void looper_set_clock_cb(void (*cb)(void));
int  looper_vocal_take_dirty(void);               // bitmask of slots changed since last call
void looper_vocal_owner(int slot, int *track, int *pat);
int  looper_track_kind(int t);
bool looper_track_has(int t, int p);

// export by real-time bounce: pass k captures tracks 3k..3k+2 (or the master when
// first >= n_tracks). Buffers are int16, loop_len each.
bool looper_bounce_start(int first);
bool looper_bounce_done(void);
int16_t *looper_bounce_buf(int i);
void looper_bounce_release(void);
