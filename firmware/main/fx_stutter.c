// Beat repeat: the last eighth note is captured on the grid and re-triggered
// on a per-step division pattern locked to the kit tempo. Presets pick the
// pattern, the mix, whether it only runs while you make sound, and whether a
// quiet metronome tick marks the beats.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>
#include <string.h>

#define RING 8192
static float rec[RING];
static float slice[RING];
static int   rp;
static int   pos, beat_len, step_len, bar_len;
static int   replen, active;
static int   click_left;
static float click_ph;
static int   step_cur;

typedef struct { const char *name; int div[8]; float wet, dry; bool gated, click; } preset_t;
static const preset_t pre[] = {
    { "1/8 pattern", { 1, 2, 2, 4, 1, 2, 4, 8 }, 0.9f, 0.25f, true,  true  },
    { "1/16 all",    { 2, 2, 2, 2, 2, 2, 2, 2 }, 0.9f, 0.25f, true,  true  },
    { "glitch ramp", { 8, 4, 2, 1, 8, 4, 2, 1 }, 0.9f, 0.10f, true,  true  },
    { "always on",   { 1, 1, 4, 4, 1, 1, 8, 8 }, 0.9f, 0.25f, false, false },
};
#define NPRE (int)(sizeof pre / sizeof pre[0])
static int pcur;

static void init(void)
{
    memset(rec, 0, sizeof rec);
    memset(slice, 0, sizeof slice);
    rp = 0; pos = 0; active = 0; click_left = 0; click_ph = 0; step_cur = -1;
    beat_len = (int)(FS * 60.0f / CONFIG_KIT_BPM);
    step_len = beat_len / 2;
    if (step_len > RING) step_len = RING;
    bar_len = beat_len * 4;
    replen = step_len;
}

static void process(const float *in, float *out, int n, const voice_t *v)
{
    const preset_t *p = &pre[pcur];
    for (int i = 0; i < n; i++) {
        rec[rp] = in[i];
        rp = (rp + 1) & (RING - 1);

        int step = pos / step_len;
        int sp = pos - step * step_len;
        if (sp == 0 && step != step_cur) {
            step_cur = step;
            // capture the previous eighth
            int start = (rp - step_len) & (RING - 1);
            for (int k = 0; k < step_len; k++) slice[k] = rec[(start + k) & (RING - 1)];
            replen = step_len / p->div[step & 7];
            active = p->gated ? v->gate : 1;
        }
        float y;
        if (active) {
            int k = sp % replen;
            float fade = 1.0f;
            const int F = 48;
            if (k < F) fade = (float)k / F;
            else if (replen - k < F) fade = (float)(replen - k) / F;
            y = slice[k] * fade * p->wet + in[i] * p->dry;
        } else {
            y = in[i];
        }
        // metronome tick on every beat, accented on the downbeat
        if (p->click) {
            if (pos % beat_len == 0) { click_left = (int)(0.004f * FS); click_ph = 0; }
            if (click_left > 0) {
                float amp = (pos < beat_len) ? 0.12f : 0.07f;
                y += amp * fast_sin01(click_ph);
                click_ph += 1500.0f / FS; if (click_ph >= 1) click_ph -= 1;
                click_left--;
            }
        }
        out[i] = y;
        if (++pos >= bar_len) pos = 0;
    }
}

static const char *preset(int idx)
{
    if (idx < 0 || idx >= NPRE) return NULL;
    pcur = idx;
    return pre[pcur].name;
}

static void status(char *buf, size_t len)
{
    int step = pos / step_len;
    snprintf(buf, len, "%s %d bpm 1/%d", pre[pcur].name, CONFIG_KIT_BPM, 8 * pre[pcur].div[step & 7]);
}

const fx_t fx_stutter = { "BEAT REPEAT", init, process, status, NULL, NULL, false, preset };
