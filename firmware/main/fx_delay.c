// Feedback delay, dotted eighth at the kit tempo, darkened repeats.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define DMAX 8192
static float buf[DMAX];
static int wp, dly;
static float lp;
static float lpc;

static void init(void)
{
    memset(buf, 0, sizeof buf);
    wp = 0; lp = 0;
    dly = (int)(FS * 60.0f / CONFIG_KIT_BPM * 0.75f);   // dotted eighth
    if (dly >= DMAX) dly = DMAX - 1;
    lpc = onepole_coef(2500.0f);
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    (void)v;
    for (int i = 0; i < n; i++) {
        float d = buf[(wp - dly + DMAX) & (DMAX - 1)];
        onepole(&lp, d, lpc);
        buf[wp] = in[i] + lp * 0.45f;
        wp = (wp + 1) & (DMAX - 1);
        out[i] = in[i] + 0.6f * d;
    }
}

static void status(char *buf_, size_t len)
{
    snprintf(buf_, len, "%d ms fb 45%%", (int)(dly * 1000 / FS));
}

const fx_t fx_delay = { "DELAY", init, process, status, NULL, NULL };
