#include "synth.h"
#include <string.h>
#include "esp_attr.h"

static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0;
}

static inline float osc(float t, float dt, int wave, float pm)
{
    switch (wave) {
    case W_SQUARE: {
        float t2 = t + 0.5f;
        if (t2 >= 1) t2 -= 1;
        return (t < 0.5f ? 1.0f : -1.0f) + polyblep(t, dt) - polyblep(t2, dt);
    }
    case W_TRI: { float x = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t; return x; }
    case W_SINE: { float p = t + pm; p -= (int)p; if (p < 0) p += 1; return fast_sin01(p); }
    default: return 2.0f * t - 1.0f - polyblep(t, dt);
    }
}

void synth_init(synth_t *s, const synth_cfg_t *cfg, int voices)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    s->nv = voices > SYNTH_MAX_VOICES ? SYNTH_MAX_VOICES : voices;
    s->cut_mul = 1;
    for (int i = 0; i < s->nv; i++) s->v[i].note = -1;
}

static synth_voice_t *alloc(synth_t *s, int note)
{
    synth_voice_t *best = NULL;
    for (int i = 0; i < s->nv; i++) if (s->v[i].note == note && s->v[i].stage && s->v[i].stage < 4) return &s->v[i];
    for (int i = 0; i < s->nv; i++) if (s->v[i].stage == 0) return &s->v[i];
    for (int i = 0; i < s->nv; i++) if (!best || s->v[i].age < best->age) best = &s->v[i];
    return best;
}

void synth_note_on(synth_t *s, int note, float vel)
{
    synth_voice_t *v = s->nv == 1 ? &s->v[0] : alloc(s, note);
    bool legato = s->nv == 1 && v->stage && v->stage < 4;
    v->freq_t = note_to_freq((float)note);
    if (!legato || s->cfg.glide_ms <= 0) v->freq = v->freq_t;
    v->glide_c = s->cfg.glide_ms > 0 ? onepole_coef(1000.0f / (TWO_PI * s->cfg.glide_ms)) : 1.0f;
    v->note = note;
    v->vel = vel;
    v->stage = 1;
    v->age = ++s->clock;
    if (!legato) { v->ph2 = 0.25f; v->phs = 0; v->phm = 0; svf_reset(&v->lpf); }
}

void synth_note_off(synth_t *s, int note)
{
    for (int i = 0; i < s->nv; i++)
        if (s->v[i].stage && s->v[i].stage < 4 && (note < 0 || s->v[i].note == note)) s->v[i].stage = 4;
}

void synth_set_expr(synth_t *s, float bend, float cut_mul, float vib)
{
    s->bend = bend; s->cut_mul = cut_mul; s->vib = vib;
}

void IRAM_ATTR synth_render(synth_t *s, float *out, int n)
{
    const synth_cfg_t *c = &s->cfg;
    const float fmul = powf(2.0f, s->bend / 12.0f);
    const float lfo_inc = 5.5f / FS;
    const float a_inc = 1.0f / (c->a_ms * 0.001f * FS + 1);
    const float d_c = 1.0f - expf(-1000.0f / (c->d_ms * FS + 1));
    const float r_c = 1.0f - expf(-1000.0f / (c->r_ms * FS + 1));
    const float det = powf(2.0f, c->detune_cents / 1200.0f);
    const float inv_fs = 1.0f / FS;

    for (int i = 0; i < s->nv; i++) {
        synth_voice_t *v = &s->v[i];
        if (!v->stage) continue;
        const float g = c->gain * v->vel;
        const float fc_base = c->cutoff + c->vel_hz * v->vel;
        for (int k = 0; k < n; k++) {
            switch (v->stage) {
            case 1: v->env += a_inc; if (v->env >= 1) { v->env = 1; v->stage = 2; } break;
            case 2: v->env += (c->sus - v->env) * d_c; if (v->env - c->sus < 0.001f) v->stage = 3; break;
            case 3: break;
            default: v->env -= v->env * r_c; if (v->env < 0.0005f) { v->env = 0; v->stage = 0; v->note = -1; } break;
            }
            if (!v->stage) break;
            v->freq += v->glide_c * (v->freq_t - v->freq);
            s->lfo += lfo_inc; if (s->lfo >= 1) s->lfo -= 1;
            float dt = v->freq * inv_fs * fmul * (1.0f + s->vib * 0.025f * fast_sin01(s->lfo));
            float pm = c->fm_amt > 0 ? c->fm_amt * v->env * fast_sin01(v->phm) : 0;
            float y = osc(v->ph1, dt, c->wave, pm) + osc(v->ph2, dt * det, c->wave, pm);
            if (c->sub > 0) y += c->sub * osc(v->phs, dt * 0.5f, W_SQUARE, 0);
            v->ph1 += dt; if (v->ph1 >= 1) v->ph1 -= 1;
            v->ph2 += dt * det; if (v->ph2 >= 1) v->ph2 -= 1;
            v->phs += dt * 0.5f; if (v->phs >= 1) v->phs -= 1;
            v->phm += dt * c->fm_ratio; if (v->phm >= 1) v->phm -= 1;
            svf_set(&v->lpf, clampf((fc_base + c->env_hz * v->env) * s->cut_mul, 40.0f, SVF_MAX_HZ), c->q);
            out[k] += svf_run(&v->lpf, y, NULL, NULL) * v->env * g;
        }
    }
}

#define P(A_,D_,SU_,R_,CUT_,ENV_,VEL_,Q_,DET_,SUB_,WV_,FR_,FA_,GL_,G_) { .a_ms=A_,.d_ms=D_,.sus=SU_,.r_ms=R_,.cutoff=CUT_,.env_hz=ENV_,.vel_hz=VEL_,.q=Q_,.detune_cents=DET_,.sub=SUB_,.wave=WV_,.fm_ratio=FR_,.fm_amt=FA_,.glide_ms=GL_,.gain=G_ }
static const synth_cfg_t PRESETS[SK_N][SYNTH_PRESETS] = {
    { // bass: SUB (clean sine with a touch of square), PLUCK, ACID, REESE, DEEP (fm)
      P(3, 200, 0.85f, 80,  1200, 0,    200, 0.7f, 0,  0.45f, W_SINE,   0, 0,     30, 0.75f),
      P(2, 110, 0.05f, 60,  250,  2600, 800, 1.6f, 5,  0.4f,  W_SAW,    0, 0,     0,  0.5f),
      P(2, 140, 0.25f, 50,  120,  2400, 700, 3.5f, 0,  0.2f,  W_SAW,    0, 0,     60, 0.42f),
      P(6, 300, 0.8f,  120, 500,  700,  300, 0.9f, 22, 0.4f,  W_SAW,    0, 0,     20, 0.36f),
      P(3, 250, 0.7f,  90,  1500, 0,    0,   0.7f, 0,  0.3f,  W_SINE,   1, 0.35f, 30, 0.7f),
    },
    { // keys: KEYS, PAD, STAB, ORGAN, BELL
      P(20, 400, 0.8f, 250, 400,  1500, 500, 0.8f, 9,  0,     W_SAW,    0, 0,     0, 0.16f),
      P(250, 800, 1.0f, 900, 250, 600,  200, 0.7f, 14, 0.2f,  W_SAW,    0, 0,     0, 0.14f),
      P(2, 250, 0.3f,  200, 900,  2500, 800, 1.4f, 3,  0,     W_SQUARE, 0, 0,     0, 0.15f),
      P(5, 100, 1.0f,  60,  2500, 0,    0,   0.7f, 2,  0.35f, W_TRI,    2, 0.12f, 0, 0.2f),
      P(2, 900, 0.0f,  700, 3000, 0,    0,   0.7f, 0,  0,     W_SINE,   3.5f, 0.6f, 0, 0.22f),
    },
    { // lead: SQUARE, SAW, SOFT, FLUTE, EPIANO
      P(6, 150, 0.75f, 120, 800,  2500, 800, 1.5f, 12, 0.25f, W_SQUARE, 0, 0,     40, 0.3f),
      P(4, 200, 0.8f,  150, 1200, 3000, 1000, 0.9f, 18, 0,    W_SAW,    0, 0,     30, 0.28f),
      P(15, 300, 0.9f, 300, 500,  400,  200, 0.6f, 0,  0,     W_TRI,    0, 0,     80, 0.34f),
      P(40, 200, 0.9f, 200, 2000, 0,    0,   0.7f, 4,  0,     W_SINE,   2, 0.08f, 60, 0.4f),
      P(2, 600, 0.2f,  400, 2500, 0,    0,   0.7f, 0,  0,     W_SINE,   1, 0.5f,  0,  0.35f),
    },
};
#undef P
static const char *PRESET_NAMES[SK_N][SYNTH_PRESETS] = {
    { "SUB", "PLUCK", "ACID", "REESE", "DEEP" }, { "KEYS", "PAD", "STAB", "ORGAN", "BELL" }, { "SQUARE", "SAW", "SOFT", "FLUTE", "EPIANO" },
};
const synth_cfg_t *synth_preset(int kind, int variant)
{
    if (kind < 0 || kind >= SK_N) kind = 0;
    if (variant < 0 || variant >= SYNTH_PRESETS) variant = 0;
    return &PRESETS[kind][variant];
}
const char *synth_preset_name(int kind, int variant)
{
    if (kind < 0 || kind >= SK_N || variant < 0 || variant >= SYNTH_PRESETS) return "?";
    return PRESET_NAMES[kind][variant];
}
