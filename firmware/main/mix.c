#include "mix.h"
#include "dsp.h"
#include <string.h>
#include <assert.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"

// ---- reverb (Freeverb-lite), buffers in PSRAM
#define NC 8
#define NA 4
static const int comb_44k[NC] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ap_44k[NA]   = { 556, 441, 341, 225 };
static float *comb[NC], *ap[NA];
static int comb_len[NC], ap_len[NA], comb_i[NC], ap_i[NA];
static float comb_lp[NC];
#define REV_FB   0.84f
#define REV_DAMP 0.25f

// ---- delay
#define DLY_MAX 32768
static float *dly;
static int dly_w, dly_len;
static float dly_lp;

// ---- sidechain, clipper, limiter
static float duck;
static mix_params_t P = { .pump = 0.4f, .drive = 1.3f, .rev_mix = 0.6f, .dly_fb = 0.45f };
static float hb[15];                     // half-band FIR for 2x oversampling
static float up_z[32], dn_z[32];         // ring buffers (16 used), index below
static int up_i, dn_i;
static float lim_gain = 1, lim_gr;
#define LIM_LA 32
static float lim_buf[LIM_LA];
static int lim_i;

static void *psram(size_t bytes)
{
    void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM);
    if (!p) p = heap_caps_calloc(1, bytes, MALLOC_CAP_8BIT);
    assert(p);
    return p;
}
// the reverb touches 12 buffers every sample; PSRAM cache misses made it the
// most expensive thing in the engine, so it lives in internal RAM (~50 KB)
static void *internal(size_t bytes)
{
    void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return p ? p : psram(bytes);
}

void mix_init(void)
{
    const float sc = FS / 44100.0f;
    for (int i = 0; i < NC; i++) { comb_len[i] = (int)(comb_44k[i] * sc); comb[i] = internal(comb_len[i] * sizeof(float)); }
    for (int i = 0; i < NA; i++) { ap_len[i] = (int)(ap_44k[i] * sc); ap[i] = internal(ap_len[i] * sizeof(float)); }
    dly = psram(DLY_MAX * sizeof(float));
    mix_set_tempo(120);
    // windowed sinc half-band: taps at odd offsets are zero except the centre
    for (int i = 0; i < 15; i++) {
        int k = i - 7;
        float w = 0.42f - 0.5f * cosf(TWO_PI * i / 14.0f) + 0.08f * cosf(2 * TWO_PI * i / 14.0f);
        hb[i] = (k == 0 ? 0.5f : sinf(0.5f * 3.14159265f * k) / (3.14159265f * k)) * w;
    }
    float sum = 0;
    for (int i = 0; i < 15; i++) sum += hb[i];
    for (int i = 0; i < 15; i++) hb[i] /= sum;
}

void mix_set_tempo(float bpm)
{
    int len = (int)(FS * 60.0f / bpm * 0.75f);      // dotted eighth
    if (len >= DLY_MAX) len = DLY_MAX - 1;
    dly_len = len;
}

void mix_kick(void) { duck = 1.0f; }
mix_params_t *mix_params(void) { return &P; }
float mix_gain_reduction(void) { return lim_gr; }

// The S3 FPU has no hardware denormals: a tail decaying toward zero turns
// into software emulation and eats the core. A tiny DC offset keeps every
// recirculating path out of that range.
#define ANTI_DENORMAL 1e-9f

// tanh without libm: Pade approximant, exact enough for a clipper
static inline float clip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float fir15(float *z, int *zi, float x)
{
    *zi = (*zi + 1) & 15;
    z[*zi] = x;
    float y = 0;
    for (int i = 0; i < 15; i++) y += hb[i] * z[(*zi - i) & 15];
    return y;
}

void IRAM_ATTR mix_process(float *const layers[MIX_N], const mix_ch_t ch[MIX_N], float *out, int n)
{
    static float rev_in[128], dly_in[128], sum[128];
    const float duck_c = 1.0f - expf(-1.0f / (0.11f * FS));
    const float lpc = onepole_coef(2500.0f);

    for (int i = 0; i < n; i++) {
        float d = 1.0f - P.pump * duck;
        duck -= duck * duck_c;
        if (duck < 1e-4f) duck = 0;
        float s = 0, r = 0, dl = 0;
        for (int l = 0; l < MIX_N; l++) {
            if (ch[l].mute) continue;
            float x = layers[l][i] * ch[l].vol;
            if (l != 0) x *= d;
            s += x; r += x * ch[l].rev; dl += x * ch[l].dly;
        }
        sum[i] = s; rev_in[i] = r + ANTI_DENORMAL; dly_in[i] = dl + ANTI_DENORMAL;
    }

    // delay: dotted eighth, dark feedback
    for (int i = 0; i < n; i++) {
        int rp = dly_w - dly_len; if (rp < 0) rp += DLY_MAX;
        float y = dly[rp];
        onepole(&dly_lp, y, lpc);
        dly[dly_w] = dly_in[i] + dly_lp * P.dly_fb;
        if (++dly_w >= DLY_MAX) dly_w = 0;
        sum[i] += y;
        rev_in[i] += y * 0.3f;          // delay tails feed the reverb a little
    }

    // reverb
    for (int i = 0; i < n; i++) {
        float x = rev_in[i] * 0.4f, acc = 0;
        for (int c = 0; c < NC; c++) {
            float y = comb[c][comb_i[c]];
            comb_lp[c] += (y - comb_lp[c]) * (1.0f - REV_DAMP);
            comb[c][comb_i[c]] = x + comb_lp[c] * REV_FB;
            if (++comb_i[c] >= comb_len[c]) comb_i[c] = 0;
            acc += y;
        }
        acc *= 0.125f;
        for (int a = 0; a < NA; a++) {
            float b = ap[a][ap_i[a]];
            float y = -acc + b;
            ap[a][ap_i[a]] = acc + b * 0.5f;
            if (++ap_i[a] >= ap_len[a]) ap_i[a] = 0;
            acc = y;
        }
        sum[i] += acc * P.rev_mix;
    }

    // 2x oversampled soft clip: upsample, shape, filter, decimate
    for (int i = 0; i < n; i++) {
        float x = sum[i] * P.drive;
        float a = fir15(up_z, &up_i, 2.0f * x), b = fir15(up_z, &up_i, ANTI_DENORMAL);
        a = clip(a); b = clip(b);
        fir15(dn_z, &dn_i, a);
        sum[i] = fir15(dn_z, &dn_i, b) * 2.0f / P.drive * 0.9f;
    }

    // limiter: this block's peak sets the gain before it is applied (the
    // 32-sample delay line gives the attack a head start), slow release
    float pk = 0;
    for (int i = 0; i < n; i++) { float a = fabsf(sum[i]); if (a > pk) pk = a; }
    float target = pk > 0.95f ? 0.95f / pk : 1.0f;
    if (target < lim_gain) lim_gain = target;
    const float rel = 1.0f - expf(-1.0f / (0.25f * FS));
    for (int i = 0; i < n; i++) {
        float y = lim_buf[lim_i];
        lim_buf[lim_i] = sum[i];
        if (++lim_i >= LIM_LA) lim_i = 0;
        out[i] = y * lim_gain;
        lim_gain += (1.0f - lim_gain) * rel;
        if (lim_gain > target) lim_gain = target;   // hold while this block is loud
    }
    lim_gr += 0.1f * ((1.0f - lim_gain) - lim_gr);
}
