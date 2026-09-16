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
enum { CMD_SELECT, CMD_TOGGLE, CMD_CLEAR_CH, CMD_CLEAR_ALL, CMD_MUTE, CMD_MIX, CMD_PARAM, CMD_REC, CMD_PERFORM, CMD_LOAD };
enum { MX_VOL, MX_PAN, MX_REV, MX_DLY, MX_N };
enum { P_BPM, P_ROOT, P_SCALE, P_STYLE, P_SWING, P_KIT, P_BASS, P_LEAD, P_PAD, P_SMODE, P_CLEAR, P_N };   // P_CLEAR: tap fires CLEAR ALL
enum { PF_OFF, PF_A, PF_B };            // perform modes: DRUMS roll; SAMPLE scratch, roll; synths bend/filter; pad, mic sends
#define JAM_STEPS 32
#define JAM_ROWS  8

typedef struct {
    int  step, bpm, sel, perform; bool rec;   // perform: PF_*
    int  rows;                                  // grid rows of the selected channel
    uint8_t grid[JAM_ROWS][JAM_STEPS];          // 1 = note/hit, 2 = reversed slice
    bool root_row[JAM_ROWS];                    // rows that are the scale root (for the display)
    char row_label[JAM_ROWS][3];
    bool mute[CH_N]; float mix[CH_N][MX_N];
    char param[P_N][12];                        // current values as text
    char scale[10];
    float mic_db; float sample_s; bool sample_ok;
    float gr;
} jam_ui_t;

// Persistent state. jam_get_state() snapshots from the UI task; jam_set_state()
// queues the copy so it is applied on the audio thread (st must stay valid).
#define JAM_MAGIC 0x4A414D31u   // "JAM1"
typedef struct {
    uint32_t magic, version;
    int32_t  bpm, style, swing, root, scale, kit, bpre, lpre, ppre;
    uint8_t  mute[CH_N]; float mix[CH_N][MX_N];
    uint8_t  drum[JAM_STEPS][3]; int8_t note[4][JAM_STEPS];
    int32_t  smp_len;
    int32_t  smode;                             // 0 pitch, 1 slice (version 2)
} jam_state_t;
void     jam_get_state(jam_state_t *st);
void     jam_set_state(const jam_state_t *st);
float   *jam_sample_buf(int *max_len);          // PSRAM sample buffer, for load/save
uint32_t jam_dirty(void);                       // bumps on every edit
uint32_t jam_sample_gen(void);                  // bumps when a new sample was recorded

void jam_cmd(int cmd, int ch, int a, int b);
void jam_expr(float x, float y);                // joystick tilt in perform mode
void jam_get_ui(jam_ui_t *u);
const char *jam_ch_name(int ch);
const char *jam_param_name(int p);
extern const fx_t fx_jam;
