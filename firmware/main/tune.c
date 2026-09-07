#include "tune.h"
#include "dsp.h"
#include <string.h>
#include "esp_attr.h"

#define L 1024
#define W 512.0f
static float dl[L];
static int   wp;
static float ph_a, ph_b;          // phasors of the main and octave shifters
static float ratio = 1, wet;
static int   target = -1;
static int   mode = TUNE_HARD;
static float ph_c, ph_d;          // harmony shifters
static const scale_t *cur_scale;
static float win[513];           // half sine window, indexed by ph * 512

void tune_init(void)
{
    memset(dl, 0, sizeof dl);
    wp = 0; ph_a = 0; ph_b = 0; ratio = 1; wet = 0; target = -1;
    for (int i = 0; i <= 512; i++) win[i] = sinf(3.14159265f * i / 512.0f);
}

int tune_target(void) { return target; }
void tune_set_mode(int m) { mode = m < 0 || m >= TUNE_N ? TUNE_HARD : m; }
const char *tune_mode_name(int m)
{
    static const char *n[TUNE_N] = { "HARD TUNE", "HARMONY", "ROBOT", "RAW" };
    return m >= 0 && m < TUNE_N ? n[m] : "?";
}

static inline float tap(float delay)
{
    float pos = (float)wp - delay;
    int i0 = (int)floorf(pos);
    float f = pos - i0;
    float a = dl[i0 & (L - 1)], b = dl[(i0 + 1) & (L - 1)];
    return a + (b - a) * f;
}

static inline float shift(float *ph, float r)
{
    *ph += (1.0f - r) / W;
    if (*ph >= 1) *ph -= 1;
    if (*ph < 0) *ph += 1;
    float p2 = *ph + 0.5f;
    if (p2 >= 1) p2 -= 1;
    return tap(*ph * W) * win[(int)(*ph * 512.0f)] + tap(p2 * W) * win[(int)(p2 * 512.0f)];
}

void IRAM_ATTR tune_process(const float *in, float *out, int n, const voice_t *v, const scale_t *sc)
{
    if (mode == TUNE_RAW) { memcpy(out, in, n * sizeof(float)); target = v->voiced ? v->note : -1; return; }
    float want = 1.0f, r3 = 1.0f, r5 = 1.0f;
    if (v->voiced && v->freq > 0) {
        if (mode == TUNE_ROBOT) target = sc->locked ? 48 + sc->root : 48;          // everything on the root
        else if (target < 0 || fabsf(v->note_f - target) > 0.7f) target = scale_snap(sc, v->note_f);
        want = clampf(note_to_freq((float)target) / v->freq, 0.5f, 2.0f);
        if (mode == TUNE_HARMONY) {
            int tri[3];
            scale_triad(sc, target, tri);
            r3 = want * note_to_freq((float)tri[1]) / note_to_freq((float)target);
            r5 = want * note_to_freq((float)tri[2]) / note_to_freq((float)target);
        }
    } else if (!v->gate) {
        target = -1;
    }
    const float rc = onepole_coef(mode == TUNE_ROBOT ? 1000.0f : 400.0f), wc = onepole_coef(30.0f);
    for (int i = 0; i < n; i++) {
        onepole(&ratio, want, rc);
        onepole(&wet, v->voiced ? 1.0f : 0.0f, wc);
        dl[wp] = in[i];
        float main = shift(&ph_a, ratio);
        float y;
        if (mode == TUNE_HARMONY) {
            float third = shift(&ph_c, r3), fifth = shift(&ph_d, r5);
            y = softclip((main + 0.6f * third + 0.5f * fifth) * 1.2f) * 0.8f;
        } else {
            float oct = shift(&ph_b, ratio * 2.0f);
            y = softclip((main + (mode == TUNE_ROBOT ? 0.0f : 0.35f) * oct) * 1.6f) * 0.8f;
        }
        out[i] = wet * y + (1.0f - wet) * in[i];
        wp = (wp + 1) & (L - 1);
    }
}
