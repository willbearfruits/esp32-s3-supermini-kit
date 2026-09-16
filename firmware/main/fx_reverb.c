// Freeverb-style reverb scaled to the kit sample rate: 8 combs, 4 allpasses.
// Presets set the tail length (comb feedback), damping and wet/dry balance.
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
static float feedback, damp, wet, dry;

typedef struct { const char *name; float feedback, damp, wet, dry; } preset_t;
static const preset_t pre[] = {
    { "room",      0.72f, 0.45f, 0.35f, 0.8f },
    { "hall",      0.84f, 0.25f, 0.50f, 0.7f },
    { "cathedral", 0.93f, 0.12f, 0.60f, 0.6f },
    { "wash",      0.97f, 0.05f, 0.80f, 0.4f },
};
#define NPRE (int)(sizeof pre / sizeof pre[0])
static int pcur;

static void apply(void)
{
    feedback = pre[pcur].feedback; damp = pre[pcur].damp; wet = pre[pcur].wet; dry = pre[pcur].dry;
}

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
    apply();
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    (void)v;
    for (int i = 0; i < n; i++) {
        float x = in[i] * 0.3f + 1e-9f;     // offset keeps the tail out of denormals
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

static const char *preset(int idx)
{
    if (idx < 0 || idx >= NPRE) return NULL;
    pcur = idx; apply();
    return pre[pcur].name;
}

static void status(char *buf, size_t len) { snprintf(buf, len, "%s  wet %d%%", pre[pcur].name, (int)(wet * 100)); }

const fx_t fx_reverb = { "REVERB", init, process, status, NULL, NULL, false, preset };
