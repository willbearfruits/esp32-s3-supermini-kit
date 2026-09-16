// Feedback delay with darkened repeats. Presets pick the time (in beats at
// the kit tempo, or milliseconds), feedback, repeat tone and mix. The buffer
// lives in PSRAM so a whole bar fits.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

#define DMAX_PSRAM 65536      // 2 s at 32 kHz
#define DMAX_INT   16384      // fallback, 0.5 s
static float *buf;
static int dmax, wp, dly;
static float lp, lpc, fb, mix;

typedef struct { const char *name; float beats, ms, fb, lp_hz, mix; } preset_t;
static const preset_t pre[] = {
    { "1/8. dark",    0.75f, 0,  0.45f, 2500, 0.6f },
    { "1/4 long",     1.0f,  0,  0.65f, 4000, 0.6f },
    { "slap 90ms",    0,     90, 0.15f, 6000, 0.7f },
    { "1/16 runaway", 0.25f, 0,  0.80f, 1800, 0.5f },
};
#define NPRE (int)(sizeof pre / sizeof pre[0])
static int pcur;

static void apply(void)
{
    const preset_t *p = &pre[pcur];
    dly = p->ms > 0 ? (int)(p->ms * FS / 1000) : (int)(FS * 60.0f / CONFIG_KIT_BPM * p->beats);
    if (dly >= dmax) dly = dmax - 1;
    if (dly < 1) dly = 1;
    lpc = onepole_coef(p->lp_hz);
    fb = p->fb; mix = p->mix;
}

static void init(void)
{
    if (!buf) {
        buf = heap_caps_malloc(DMAX_PSRAM * sizeof(float), MALLOC_CAP_SPIRAM);
        dmax = DMAX_PSRAM;
        if (!buf) { buf = malloc(DMAX_INT * sizeof(float)); dmax = DMAX_INT; }
    }
    memset(buf, 0, dmax * sizeof(float));
    wp = 0; lp = 0;
    apply();
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    (void)v;
    const int mask = dmax - 1;
    for (int i = 0; i < n; i++) {
        float d = buf[(wp - dly) & mask];
        onepole(&lp, d, lpc);
        buf[wp] = in[i] + lp * fb + 1e-9f;
        wp = (wp + 1) & mask;
        out[i] = in[i] + mix * d;
    }
}

static const char *preset(int idx)
{
    if (idx < 0 || idx >= NPRE) return NULL;
    pcur = idx; apply();
    return pre[pcur].name;
}

static void status(char *buf_, size_t len)
{
    snprintf(buf_, len, "%s %d ms fb %d%%", pre[pcur].name, (int)(dly * 1000 / FS), (int)(fb * 100));
}

const fx_t fx_delay = { "DELAY", init, process, status, NULL, NULL, false, preset };
