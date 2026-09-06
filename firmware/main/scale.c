#include "scale.h"
#include <math.h>
#include <stdio.h>

static const int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int MINOR[7] = { 0, 2, 3, 5, 7, 8, 10 };
// Krumhansl-Kessler key profiles
static const float P_MAJ[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
static const float P_MIN[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
static const char *NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static const int *steps(const scale_t *s) { return s->minor ? MINOR : MAJOR; }

static int degree_of(const scale_t *s, int note)
{
    int pc = ((note - s->root) % 12 + 12) % 12;
    const int *st = steps(s);
    for (int i = 0; i < 7; i++) if (st[i] == pc) return i;
    return -1;
}

bool scale_contains(const scale_t *s, int note)
{
    return !s->locked || degree_of(s, note) >= 0;
}

int scale_snap(const scale_t *s, float note_f)
{
    int n = (int)lroundf(note_f);
    if (!s->locked) return n;
    int best = n;
    float bd = 1e9f;
    for (int c = n - 3; c <= n + 3; c++) {
        if (degree_of(s, c) < 0) continue;
        float d = fabsf(c - note_f);
        if (d < bd - 1e-4f || (fabsf(d - bd) < 1e-4f && c < best)) { bd = d; best = c; }
    }
    return best;
}

void scale_triad(const scale_t *s, int root_note, int out[3])
{
    if (!s->locked) { out[0] = root_note; out[1] = root_note + 4; out[2] = root_note + 7; return; }
    int d = degree_of(s, root_note);
    if (d < 0) { root_note = scale_snap(s, (float)root_note); d = degree_of(s, root_note); }
    const int *st = steps(s);
    int third = st[(d + 2) % 7] - st[d], fifth = st[(d + 4) % 7] - st[d];
    if (third < 0) third += 12;
    if (fifth < 0) fifth += 12;
    out[0] = root_note; out[1] = root_note + third; out[2] = root_note + fifth;
}

static float correlate(const float *w, const float *p, int root)
{
    float mw = 0, mp = 0;
    for (int i = 0; i < 12; i++) { mw += w[i]; mp += p[i]; }
    mw /= 12; mp /= 12;
    float num = 0, dw = 0, dp = 0;
    for (int i = 0; i < 12; i++) {
        float a = w[i] - mw, b = p[(i - root + 12) % 12] - mp;
        num += a * b; dw += a * a; dp += b * b;
    }
    return dw > 0 ? num / sqrtf(dw * dp) : 0;
}

bool scale_detect(const float w[12], scale_t *s)
{
    float total = 0;
    for (int i = 0; i < 12; i++) total += w[i];
    if (total <= 0) return false;
    float best = -2;
    for (int root = 0; root < 12; root++) {
        float cm = correlate(w, P_MAJ, root), cn = correlate(w, P_MIN, root);
        if (cm > best) { best = cm; s->root = root; s->minor = false; }
        if (cn > best) { best = cn; s->root = root; s->minor = true; }
    }
    s->locked = true;
    return true;
}

const char *scale_name(const scale_t *s, char *buf, int len)
{
    if (!s->locked) snprintf(buf, len, "free");
    else snprintf(buf, len, "%s %s", NAMES[s->root], s->minor ? "min" : "maj");
    return buf;
}
