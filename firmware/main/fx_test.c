// Plain mic -> DAC pass-through, with the voice tracker readout on screen.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>

static voice_t last;

static void init(void) {}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    last = *v;
    for (int i = 0; i < n; i++) out[i] = in[i];
}

static void status(char *buf, size_t len)
{
    char nm[5];
    if (last.voiced)
        snprintf(buf, len, "%s %.0fHz c%.0f%%", note_name(last.note, nm), last.freq, last.conf * 100);
    else
        snprintf(buf, len, "%s %.0f dB", last.gate ? "voice" : "quiet", last.db);
}

const fx_t fx_test = { "MIC TEST", init, process, status, NULL, NULL };
