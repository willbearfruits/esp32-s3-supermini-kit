// JAM: a six-channel step sequencer with a scale-locked note grid, a sampler
// you can scratch, and a mixer, driven from the encoder, BOOT and joystick.
// Channels: DRUMS (kick/snare/hat lanes), BASS, LEAD, PAD (chords), SAMPLE,
// MIC (live mic, only audible while selected; records the sample). The engine
// runs inside the audio task as fx_jam; the UI sends commands and reads a
// snapshot with jam_get_ui().
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fx.h"

enum { CH_DRUMS, CH_BASS, CH_LEAD, CH_PAD, CH_SAMPLE, CH_MIC, CH_N };
enum { CMD_SELECT, CMD_TOGGLE, CMD_CLEAR_CH, CMD_CLEAR_ALL, CMD_MUTE, CMD_MIX, CMD_PARAM, CMD_REC, CMD_PERFORM };
enum { MX_VOL, MX_PAN, MX_REV, MX_DLY, MX_N };
enum { P_BPM, P_ROOT, P_SCALE, P_STYLE, P_SWING, P_KIT, P_BASS, P_LEAD, P_PAD, P_CLEAR, P_N };   // P_CLEAR: tap fires CLEAR ALL
#define JAM_STEPS 32
#define JAM_ROWS  8

typedef struct {
    int  step, bpm, sel; bool perform, rec;
    int  rows;                                  // grid rows of the selected channel
    uint8_t grid[JAM_ROWS][JAM_STEPS];          // 1 = note/hit
    bool root_row[JAM_ROWS];                    // rows that are the scale root (for the display)
    char row_label[JAM_ROWS][3];
    bool mute[CH_N]; float mix[CH_N][MX_N];
    char param[P_N][12];                        // current values as text
    char scale[10];
    float mic_db; float sample_s; bool sample_ok;
    float gr;
} jam_ui_t;

void jam_cmd(int cmd, int ch, int a, int b);
void jam_expr(float x, float y);                // joystick tilt in perform mode
void jam_get_ui(jam_ui_t *u);
const char *jam_ch_name(int ch);
const char *jam_param_name(int p);
extern const fx_t fx_jam;
