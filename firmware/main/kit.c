#include "kit.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "kit";

typedef struct {
    const char *name;
    float k_f0, k_f1, k_pdec, k_adec, k_click, k_drive;       // kick: start/end Hz, pitch and amp decays (s)
    float s_bp_hz, s_bp_q, s_ndec, s_tone_hz, s_tdec, s_tone;  // snare
    float h_hp_hz, h_dec;                                      // hat
} kit_def_t;

static const kit_def_t KITS[KIT_SYNTH_N] = {
    { "TIGHT", 180, 48, 0.035f, 0.16f, 0.4f, 1.0f,  1800, 1.2f, 0.075f, 190, 0.035f, 0.6f,  7000, 0.022f },
    { "808",   120, 42, 0.060f, 0.42f, 0.2f, 1.6f,  1200, 0.9f, 0.110f, 175, 0.050f, 0.3f,  8500, 0.035f },
    { "LOFI",  260, 60, 0.020f, 0.09f, 0.8f, 2.2f,  2600, 2.0f, 0.045f, 240, 0.020f, 0.9f,  4000, 0.012f },
    { "TRAP",  90,  38, 0.090f, 0.60f, 0.1f, 2.0f,  1500, 0.6f, 0.140f, 0,   0.010f, 0.0f,  9000, 0.018f },
};
static kit_sample_t user[DRUM_N];
static bool user_ok;
static biquad_t an_lo, an_hi;

static float decay(float tau_s) { return expf(-1.0f / (tau_s * FS)); }

void drums_set_kit(drums_t *d, int kit)
{
    if (kit < 0 || kit >= KIT_N) kit = 0;
    if (kit == KIT_USER && !user_ok) kit = 0;
    d->kit = kit;
    const kit_def_t *k = &KITS[kit < KIT_SYNTH_N ? kit : 0];
    d->k_amp_c = decay(k->k_adec); d->k_pitch_c = decay(k->k_pdec);
    d->s_amp_c = decay(k->s_ndec); d->s_tone_c = decay(k->s_tdec);
    d->h_amp_c = decay(k->h_dec);
    svf_set(&d->snare_bp, k->s_bp_hz, k->s_bp_q);
    svf_set(&d->hat_hp, k->h_hp_hz, 0.7f);
}

void drums_init(drums_t *d, int kit)
{
    memset(d, 0, sizeof *d);
    d->seed = 12345;
    drums_set_kit(d, kit);
}

void drums_trigger(drums_t *d, int type, float vel)
{
    if (type < 0 || type >= DRUM_N) return;
    drum_voice_t *v = &d->v[type];
    v->on = true; v->vel = vel; v->amp = 1; v->amp2 = 1; v->penv = 1; v->ph = 0; v->ph2 = 0; v->pos = 0;
}

static inline float sample_tick(drum_voice_t *v, const kit_sample_t *s)
{
    int i = (int)v->pos;
    if (i + 1 >= s->len) { v->on = false; return 0; }
    float f = v->pos - i;
    float y = (s->data[i] + (s->data[i + 1] - s->data[i]) * f) / 32768.0f;
    v->pos += s->step;
    return y * v->vel;
}

void IRAM_ATTR drums_render(drums_t *d, float *out, int n, float gain)
{
    const float inv_fs = 1.0f / FS;
    drum_voice_t *k = &d->v[DRUM_KICK], *s = &d->v[DRUM_SNARE], *h = &d->v[DRUM_HAT];
    if (d->kit == KIT_USER) {
        for (int i = 0; i < n; i++) {
            float y = 0;
            for (int t = 0; t < DRUM_N; t++) if (d->v[t].on) y += sample_tick(&d->v[t], &user[t]);
            out[i] += y * gain;
        }
        return;
    }
    const kit_def_t *kd = &KITS[d->kit];
    for (int i = 0; i < n; i++) {
        float y = 0;
        float noise = frand(&d->seed);
        if (k->on) {
            float f = kd->k_f1 + (kd->k_f0 - kd->k_f1) * k->penv;
            k->ph += f * inv_fs; if (k->ph >= 1) k->ph -= 1;
            float body = fast_sin01(k->ph) * k->amp * kd->k_drive;
            body = body > 1 ? 1 : (body < -1 ? -1 : body);          // 808 style clip
            y += (body + (k->penv > 0.9f ? kd->k_click * noise : 0.0f)) * k->vel * 1.1f;
            k->amp *= d->k_amp_c; k->penv *= d->k_pitch_c;
            if (k->amp < 0.002f) k->on = false;
        }
        float bp;
        svf_run(&d->snare_bp, noise, &bp, NULL);
        if (s->on) {
            s->ph += kd->s_tone_hz * inv_fs; if (s->ph >= 1) s->ph -= 1;
            y += (bp * s->amp * 1.6f + fast_sin01(s->ph) * s->amp2 * kd->s_tone) * s->vel;
            s->amp *= d->s_amp_c; s->amp2 *= d->s_tone_c;
            if (s->amp < 0.002f) s->on = false;
        }
        float hp;
        svf_run(&d->hat_hp, noise, NULL, &hp);
        if (h->on) {
            y += hp * h->amp * h->vel * 1.1f;
            h->amp *= d->h_amp_c;
            if (h->amp < 0.002f) h->on = false;
        }
        out[i] += y * gain;
    }
}

const char *kit_name(int kit)
{
    if (kit == KIT_USER) return user_ok ? "USER" : "USER?";
    return kit >= 0 && kit < KIT_SYNTH_N ? KITS[kit].name : "?";
}

bool kit_user_loaded(void) { return user_ok; }

// ---- WAV loader: 16-bit PCM, mono or stereo (left taken), any rate, max 2 s
static bool load_wav(const char *path, kit_sample_t *s)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return false; }
    int rate = 0, ch = 0, bits = 0;
    bool ok = false;
    while (1) {
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) break;
        uint32_t len = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t fm[16];
            if (fread(fm, 1, 16, f) != 16) break;
            ch = fm[2]; rate = fm[4] | fm[5] << 8 | fm[6] << 16; bits = fm[14];
            fseek(f, len - 16, SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            if (bits != 16 || ch < 1 || rate < 8000) break;
            int frames = len / (2 * ch);
            int maxf = 2 * rate;
            if (frames > maxf) frames = maxf;
            int16_t *buf = heap_caps_malloc(frames * sizeof(int16_t), MALLOC_CAP_SPIRAM);
            if (!buf) break;
            int16_t fr[2];
            for (int i = 0; i < frames; i++) {
                if (fread(fr, 2, ch > 2 ? 2 : ch, f) < 1) { frames = i; break; }
                if (ch > 2) fseek(f, 2 * (ch - 2), SEEK_CUR);
                buf[i] = fr[0];
            }
            if (s->data) free(s->data);
            s->data = buf; s->len = frames; s->step = (float)rate / FS;
            ok = true;
            break;
        } else fseek(f, (len + 1) & ~1u, SEEK_CUR);
    }
    fclose(f);
    return ok;
}

int kit_load_user(const char *dir)
{
    static const char *names[DRUM_N] = { "kick.wav", "snare.wav", "hat.wav" };
    int got = 0;
    for (int t = 0; t < DRUM_N; t++) {
        char path[64];
        snprintf(path, sizeof path, "%s/%s", dir, names[t]);
        if (load_wav(path, &user[t])) { got++; ESP_LOGI(TAG, "loaded %s: %d samples", path, user[t].len); }
    }
    user_ok = got == DRUM_N;
    return got;
}

// ---- classifier
void drums_analyser_init(void)
{
    biquad_lowpass(&an_lo, 250.0f, 0.707f);
    biquad_highpass(&an_hi, 2500.0f, 0.707f);
}

void drums_analyse(const float *in, int n, drum_bands_t *b)
{
    float lo = 0, hi = 0, all = 0;
    for (int i = 0; i < n; i++) {
        float l = biquad_run(&an_lo, in[i]), h = biquad_run(&an_hi, in[i]);
        lo += l * l; hi += h * h; all += in[i] * in[i];
    }
    b->lo = lo; b->hi = hi; b->all = all;
}

int drums_classify(const drum_bands_t *b, float *lo_ratio, float *hi_ratio)
{
    float all = b->all > 1e-12f ? b->all : 1e-12f;
    float lr = b->lo / all, hr = b->hi / all;
    if (lo_ratio) *lo_ratio = lr;
    if (hi_ratio) *hi_ratio = hr;
    if (hr > 0.35f) return DRUM_HAT;
    if (lr > 0.40f) return DRUM_KICK;
    return DRUM_SNARE;
}

const char *drums_name(int type)
{
    static const char *n[DRUM_N] = { "KICK", "SNARE", "HAT" };
    return type >= 0 && type < DRUM_N ? n[type] : "?";
}
