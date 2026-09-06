// Rhythmic stutter (beat repeat): while you make sound, the last eighth note
// is re-triggered on a 1/8, 1/16, 1/32 pattern locked to the kit tempo. A
// quiet metronome tick marks the beats so you can play in time.
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
static const int pattern_div[8] = { 1, 2, 2, 4, 1, 2, 4, 8 };   // per eighth step
static int  step_cur;

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
            replen = step_len / pattern_div[step & 7];
            active = v->gate;
        }
        float y;
        if (active) {
            int k = sp % replen;
            float fade = 1.0f;
            const int F = 48;
            if (k < F) fade = (float)k / F;
            else if (replen - k < F) fade = (float)(replen - k) / F;
            y = slice[k] * fade * 0.9f + in[i] * 0.25f;
        } else {
            y = in[i];
        }
        // metronome tick on every beat, accented on the downbeat
        if (pos % beat_len == 0) { click_left = (int)(0.004f * FS); click_ph = 0; }
        if (click_left > 0) {
            float amp = (pos < beat_len) ? 0.12f : 0.07f;
            y += amp * sinf(click_ph);
            click_ph += TWO_PI * 1500.0f / FS;
            click_left--;
        }
        out[i] = y;
        if (++pos >= bar_len) pos = 0;
    }
}

static void status(char *buf, size_t len)
{
    int step = pos / step_len;
    snprintf(buf, len, "%d bpm  beat %d  1/%d", CONFIG_KIT_BPM, step / 2 + 1, 8 * pattern_div[step & 7]);
}

const fx_t fx_stutter = { "STUTTER", init, process, status, NULL, NULL };
