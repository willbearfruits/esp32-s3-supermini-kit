// Freeverb-style reverb scaled to the kit sample rate: 8 combs, 4 allpasses.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define NC 8
#define NA 4
// Freeverb tunings at 44.1 kHz, scaled at init
static const int comb_44k[NC] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ap_44k[NA]   = { 556, 441, 341, 225 };

static float cbuf[NC][700], abuf[NA][260];
static int   clen[NC], alen[NA], cpos[NC], apos[NA];
static float cfilt[NC];
static const float feedback = 0.84f, damp = 0.25f, wet = 0.5f, dry = 0.7f;

static void init(void)
{
    float sc = FS / 44100.0f;
    for (int i = 0; i < NC; i++) {
        clen[i] = (int)(comb_44k[i] * sc); if (clen[i] > 699) clen[i] = 699;
        cpos[i] = 0; cfilt[i] = 0;
        memset(cbuf[i], 0, sizeof cbuf[i]);
    }
    for (int i = 0; i < NA; i++) {
        alen[i] = (int)(ap_44k[i] * sc); if (alen[i] > 259) alen[i] = 259;
        apos[i] = 0;
        memset(abuf[i], 0, sizeof abuf[i]);
    }
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    (void)v;
    for (int i = 0; i < n; i++) {
        float x = in[i] * 0.3f;
        float acc = 0;
        for (int c = 0; c < NC; c++) {
            float y = cbuf[c][cpos[c]];
            cfilt[c] = y * (1 - damp) + cfilt[c] * damp;
            cbuf[c][cpos[c]] = x + cfilt[c] * feedback;
            if (++cpos[c] >= clen[c]) cpos[c] = 0;
            acc += y;
        }
        for (int a = 0; a < NA; a++) {
            float b = abuf[a][apos[a]];
            float y = -acc + b;
            abuf[a][apos[a]] = acc + b * 0.5f;
            if (++apos[a] >= alen[a]) apos[a] = 0;
            acc = y;
        }
        out[i] = dry * in[i] + wet * acc;
    }
}

static void status(char *buf, size_t len) { snprintf(buf, len, "hall  wet 50%%"); }

const fx_t fx_reverb = { "REVERB", init, process, status, NULL, NULL };
