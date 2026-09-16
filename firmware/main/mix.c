#include "mix.h"
#include "dsp.h"
#include <string.h>
#include <assert.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"

#define NC 8
#define NA 4
static const int comb_44k[NC] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ap_44k[NA]   = { 556, 441, 341, 225 };
static float *comb[NC], *ap[NA];
static int comb_len[NC], ap_len[NA], comb_i[NC], ap_i[NA];
static float comb_lp[NC];
#define REV_FB   0.84f
#define REV_DAMP 0.25f
#define WIDTH_N  320                       // 10 ms right-channel offset for width
static float width[WIDTH_N]; static int width_i;

#define DLY_MAX 32768
static float *dly_l, *dly_r;
static int dly_w, dly_len;
static float dly_lp_l, dly_lp_r;

#define ANTI_DENORMAL 1e-9f
#define CHO_N 1024
typedef struct {
    biquad_t hp; float lp; float last_hz;
    float lfo;                       // effect LFO phase 0..1
    float cho[CHO_N]; int cho_i;     // chorus line
    float ap[4];                     // phaser allpass states
    float hold; int hold_n;          // crusher sample-and-hold
    svf_t wob;
} ch_state_t;
static ch_state_t cs[MIX_MAX];
static float beat_hz = 2;            // for tempo-synced LFOs

const char *mix_fx_name(int fx)
{
    static const char *n[FX_N] = { "none", "DRIVE", "CRUSH", "CHORUS", "PHASER", "WOBBLE", "TREMOLO" };
    return fx >= 0 && fx < FX_N ? n[fx] : "?";
}

static inline float fx_run(ch_state_t *st, int fx, float amt, float x)
{
    switch (fx) {
    case FX_DRIVE: {
        float g = 1.0f + amt * 12.0f;
        float y = x * g;
        if (y > 3) y = 3;
        if (y < -3) y = -3;
        float y2 = y * y;
        return y * (27.0f + y2) / (27.0f + 9.0f * y2) / (0.6f + 0.4f * g * 0.25f);
    }
    case FX_CRUSH: {
        int dec = 1 + (int)(amt * 12);
        if (++st->hold_n >= dec) { st->hold_n = 0; float q = 4.0f + (1.0f - amt) * 60.0f; st->hold = (int)(x * q) / q; }
        return st->hold;
    }
    case FX_CHORUS: {
        st->lfo += 0.6f / FS; if (st->lfo >= 1) st->lfo -= 1;
        float d = (8.0f + 6.0f * fast_sin01(st->lfo)) * FS * 0.001f;
        float pos = st->cho_i - d;
        int i0 = (int)pos; float f = pos - i0;
        float a = st->cho[(i0 + CHO_N) & (CHO_N - 1)], b = st->cho[(i0 + 1 + CHO_N) & (CHO_N - 1)];
        st->cho[st->cho_i] = x + ANTI_DENORMAL;
        st->cho_i = (st->cho_i + 1) & (CHO_N - 1);
        return x + (a + (b - a) * f) * amt;
    }
    case FX_PHASER: {
        st->lfo += 0.35f / FS; if (st->lfo >= 1) st->lfo -= 1;
        float c = 0.2f + 0.7f * (0.5f + 0.5f * fast_sin01(st->lfo));   // allpass coefficient sweep
        float y = x + ANTI_DENORMAL;
        for (int i = 0; i < 4; i++) { float t = y - c * st->ap[i]; y = c * t + st->ap[i]; st->ap[i] = t; }
        return x + y * amt;
    }
    case FX_WOBBLE: {
        st->lfo += beat_hz * 0.5f / FS; if (st->lfo >= 1) st->lfo -= 1;     // one wobble per half note
        float fc = 150.0f * powf(2.0f, 4.5f * (0.5f + 0.5f * fast_sin01(st->lfo)) * amt);
        svf_set(&st->wob, fc, 2.5f);
        return svf_run(&st->wob, x, NULL, NULL);
    }
    case FX_TREMOLO: {
        st->lfo += beat_hz * 2.0f / FS; if (st->lfo >= 1) st->lfo -= 1;     // eighths
        return x * (1.0f - amt * (0.5f + 0.5f * fast_sin01(st->lfo)));
    }
    default: return x;
    }
}
static float master_lp_l, master_lp_r;

static float duck;
static mix_params_t P = { .pump = 0.4f, .drive = 1.3f, .tone = 0, .rev_mix = 0.6f, .dly_fb = 0.45f, .oversample = true };
#define NT 11
static float hb[NT];
static float up_z[2][16], dn_z[2][16];
static int up_i[2], dn_i[2];
static float lim_gain = 1, lim_gr;
#define LIM_LA 32
static float lim_buf[2][LIM_LA];
static int lim_i;

static void *psram(size_t bytes)
{
    void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM);
    if (!p) p = heap_caps_calloc(1, bytes, MALLOC_CAP_8BIT);
    assert(p);
    return p;
}
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
    dly_l = psram(DLY_MAX * sizeof(float));
    dly_r = psram(DLY_MAX * sizeof(float));
    mix_set_tempo(120);
    for (int i = 0; i < NT; i++) {
        int k = i - NT / 2;
        float w = 0.42f - 0.5f * cosf(TWO_PI * i / (NT - 1)) + 0.08f * cosf(2 * TWO_PI * i / (NT - 1));
        hb[i] = (k == 0 ? 0.5f : sinf(0.5f * 3.14159265f * k) / (3.14159265f * k)) * w;
    }
    float sum = 0;
    for (int i = 0; i < NT; i++) sum += hb[i];
    for (int i = 0; i < NT; i++) hb[i] /= sum;
    for (int c = 0; c < MIX_MAX; c++) { biquad_highpass(&cs[c].hp, 40, 0.707f); cs[c].last_hz = 40; }
}

void mix_set_tempo(float bpm)
{
    int len = (int)(FS * 60.0f / bpm * 0.75f);
    if (len >= DLY_MAX) len = DLY_MAX - 1;
    dly_len = len;
    beat_hz = bpm / 60.0f;
}

void mix_kick(void) { duck = 1.0f; }
mix_params_t *mix_params(void) { return &P; }
float mix_gain_reduction(void) { return lim_gr; }

static inline float clip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float fir(float *z, int *zi, float x)
{
    *zi = (*zi + 1) & 15;
    z[*zi] = x;
    float y = 0;
    for (int i = 0; i < NT; i++) y += hb[i] * z[(*zi - i) & 15];
    return y;
}

static inline float oversampled_clip(int c, float x)
{
    float a = fir(up_z[c], &up_i[c], 2.0f * x), b = fir(up_z[c], &up_i[c], ANTI_DENORMAL);
    fir(dn_z[c], &dn_i[c], clip(a));
    return fir(dn_z[c], &dn_i[c], clip(b)) * 2.0f;
}

void IRAM_ATTR mix_process(float *const tracks[], const mix_ch_t ch[], int nch, float *out, int n)
{
    static float L[128], R[128], rev_in[128], dly_in[128], dk[128];
    const float duck_c = 1.0f - expf(-1.0f / (0.11f * FS));
    const float lpc = onepole_coef(2500.0f), tone_c = onepole_coef(900.0f);
    memset(L, 0, n * sizeof(float)); memset(R, 0, n * sizeof(float));
    memset(rev_in, 0, n * sizeof(float)); memset(dly_in, 0, n * sizeof(float));
    for (int i = 0; i < n; i++) { dk[i] = 1.0f - P.pump * duck; duck -= duck * duck_c; }
    if (duck < 1e-4f) duck = 0;

    // per-track: low cut, tone, pan, sends
    for (int c = 0; c < nch && c < MIX_MAX; c++) {
        if (ch[c].mute) continue;
        ch_state_t *st = &cs[c];
        if (ch[c].lowcut_hz > 0 && fabsf(ch[c].lowcut_hz - st->last_hz) > 1) { biquad_highpass(&st->hp, ch[c].lowcut_hz, 0.707f); st->last_hz = ch[c].lowcut_hz; }
        float th = (ch[c].pan + 1.0f) * 0.25f * 3.14159265f;
        float gl = cosf(th) * ch[c].vol, gr = sinf(th) * ch[c].vol;
        float tone = ch[c].tone * 0.6f;
        for (int i = 0; i < n; i++) {
            float x = tracks[c][i];
            if (ch[c].lowcut_hz > 0) x = biquad_run(&st->hp, x);
            if (tone != 0) { onepole(&st->lp, x, tone_c); x = st->lp * (1.0f - tone) + (x - st->lp) * (1.0f + tone); }
            if (ch[c].fx) x = fx_run(st, ch[c].fx, ch[c].fx_amt, x);
            if (ch[c].duck) x *= dk[i];
            L[i] += x * gl; R[i] += x * gr;
            rev_in[i] += x * ch[c].rev; dly_in[i] += x * ch[c].dly;
        }
    }

    // ping-pong delay
    for (int i = 0; i < n; i++) {
        int rp = dly_w - dly_len; if (rp < 0) rp += DLY_MAX;
        float yl = dly_l[rp], yr = dly_r[rp];
        onepole(&dly_lp_l, yl, lpc); onepole(&dly_lp_r, yr, lpc);
        dly_l[dly_w] = dly_in[i] + ANTI_DENORMAL + dly_lp_r * P.dly_fb;
        dly_r[dly_w] = ANTI_DENORMAL + dly_lp_l * P.dly_fb;
        if (++dly_w >= DLY_MAX) dly_w = 0;
        L[i] += yl; R[i] += yr;
        rev_in[i] += (yl + yr) * 0.15f;
    }

    // reverb, mono tank with a widened return
    for (int i = 0; i < n; i++) {
        float x = rev_in[i] * 0.4f + ANTI_DENORMAL, acc = 0;
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
        float late = width[width_i];
        width[width_i] = acc;
        if (++width_i >= WIDTH_N) width_i = 0;
        L[i] += acc * P.rev_mix; R[i] += late * P.rev_mix;
    }

    // master: tilt tone, oversampled clip, limiter
    float tone = P.tone * 0.5f, pk = 0;
    for (int i = 0; i < n; i++) {
        float l = L[i], r = R[i];
        if (tone != 0) {
            onepole(&master_lp_l, l, tone_c); l = master_lp_l * (1 - tone) + (l - master_lp_l) * (1 + tone);
            onepole(&master_lp_r, r, tone_c); r = master_lp_r * (1 - tone) + (r - master_lp_r) * (1 + tone);
        }
        if (P.oversample) { l = oversampled_clip(0, l * P.drive) / P.drive * 0.9f; r = oversampled_clip(1, r * P.drive) / P.drive * 0.9f; }
        else { l = softclip(l * P.drive) / P.drive * 0.9f; r = softclip(r * P.drive) / P.drive * 0.9f; }
        L[i] = l; R[i] = r;
        float a = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
        if (a > pk) pk = a;
    }
    float target = pk > 0.95f ? 0.95f / pk : 1.0f;
    if (target < lim_gain) lim_gain = target;
    const float rel = 1.0f - expf(-1.0f / (0.25f * FS));
    for (int i = 0; i < n; i++) {
        float yl = lim_buf[0][lim_i], yr = lim_buf[1][lim_i];
        lim_buf[0][lim_i] = L[i]; lim_buf[1][lim_i] = R[i];
        if (++lim_i >= LIM_LA) lim_i = 0;
        out[2 * i] = yl * lim_gain; out[2 * i + 1] = yr * lim_gain;
        lim_gain += (1.0f - lim_gain) * rel;
        if (lim_gain > target) lim_gain = target;
    }
    lim_gr += 0.1f * ((1.0f - lim_gain) - lim_gr);
}
