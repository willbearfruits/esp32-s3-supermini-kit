// C interface to the Faust synth voice (faust/jam_voice.dsp) for the JAM
// engine: presets by kind, note on/off, expression, render.
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct fvoice fvoice_t;
enum { FV_BASS, FV_LEAD, FV_KIND_N };
#define FV_PRESETS 5
fvoice_t   *fvoice_new(int kind, int preset);
void        fvoice_preset(fvoice_t *v, int kind, int preset);
const char *fvoice_preset_name(int kind, int preset);
void        fvoice_note(fvoice_t *v, float freq, float vel, bool on);   // off ignores freq
void        fvoice_bright(fvoice_t *v, float mul);                       // cutoff multiplier 0.2..4
void        fvoice_render(fvoice_t *v, float *out, int n);               // adds into out
#ifdef __cplusplus
}
#endif
