#include "synth.h"
#include <string.h>
#include "esp_attr.h"

static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0;
}

static inline float osc(float t, float dt, bool square)
{
    if (!square) return 2.0f * t - 1.0f - polyblep(t, dt);
    float t2 = t + 0.5f;
    if (t2 >= 1) t2 -= 1;
    return (t < 0.5f ? 1.0f : -1.0f) + polyblep(t, dt) - polyblep(t2, dt);
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
    if (!legato) { v->ph2 = 0.25f; v->phs = 0; svf_reset(&v->lpf); }
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
            float y = osc(v->ph1, dt, c->square) + osc(v->ph2, dt * det, c->square);
            if (c->sub > 0) y += c->sub * osc(v->phs, dt * 0.5f, true);
            v->ph1 += dt; if (v->ph1 >= 1) v->ph1 -= 1;
            v->ph2 += dt * det; if (v->ph2 >= 1) v->ph2 -= 1;
            v->phs += dt * 0.5f; if (v->phs >= 1) v->phs -= 1;
            svf_set(&v->lpf, clampf((fc_base + c->env_hz * v->env) * s->cut_mul, 40.0f, SVF_MAX_HZ), c->q);
            out[k] += svf_run(&v->lpf, y, NULL, NULL) * v->env * g;
        }
    }
}

static const synth_cfg_t PRESETS[SK_N][SYNTH_PRESETS] = {
    { // bass
      { .a_ms = 4, .d_ms = 180, .sus = 0.7f, .r_ms = 60, .cutoff = 150, .env_hz = 900, .vel_hz = 300, .q = 1.2f, .detune_cents = 4, .sub = 0.6f, .square = false, .glide_ms = 25, .gain = 0.45f },
      { .a_ms = 2, .d_ms = 120, .sus = 0.2f, .r_ms = 40, .cutoff = 120, .env_hz = 2200, .vel_hz = 600, .q = 3.5f, .detune_cents = 0, .sub = 0.3f, .square = false, .glide_ms = 60, .gain = 0.4f },
      { .a_ms = 3, .d_ms = 90, .sus = 0.0f, .r_ms = 80, .cutoff = 300, .env_hz = 1800, .vel_hz = 500, .q = 1.0f, .detune_cents = 6, .sub = 0.5f, .square = true, .glide_ms = 0, .gain = 0.42f },
    },
    { // keys (poly)
      { .a_ms = 30, .d_ms = 400, .sus = 0.8f, .r_ms = 250, .cutoff = 400, .env_hz = 1500, .vel_hz = 500, .q = 0.8f, .detune_cents = 9, .sub = 0, .square = false, .glide_ms = 0, .gain = 0.16f },
      { .a_ms = 250, .d_ms = 800, .sus = 1.0f, .r_ms = 900, .cutoff = 250, .env_hz = 600, .vel_hz = 200, .q = 0.7f, .detune_cents = 14, .sub = 0.2f, .square = false, .glide_ms = 0, .gain = 0.14f },
      { .a_ms = 2, .d_ms = 250, .sus = 0.3f, .r_ms = 200, .cutoff = 900, .env_hz = 2500, .vel_hz = 800, .q = 1.4f, .detune_cents = 3, .sub = 0, .square = true, .glide_ms = 0, .gain = 0.15f },
    },
    { // lead
      { .a_ms = 6, .d_ms = 150, .sus = 0.75f, .r_ms = 120, .cutoff = 800, .env_hz = 2500, .vel_hz = 800, .q = 1.5f, .detune_cents = 12, .sub = 0.25f, .square = true, .glide_ms = 40, .gain = 0.3f },
      { .a_ms = 4, .d_ms = 200, .sus = 0.8f, .r_ms = 150, .cutoff = 1200, .env_hz = 3000, .vel_hz = 1000, .q = 0.9f, .detune_cents = 18, .sub = 0, .square = false, .glide_ms = 30, .gain = 0.28f },
      { .a_ms = 15, .d_ms = 300, .sus = 0.9f, .r_ms = 300, .cutoff = 500, .env_hz = 400, .vel_hz = 200, .q = 0.6f, .detune_cents = 0, .sub = 0, .square = false, .glide_ms = 80, .gain = 0.32f },
    },
};
static const char *PRESET_NAMES[SK_N][SYNTH_PRESETS] = {
    { "SUB", "ACID", "PLUCK" }, { "KEYS", "PAD", "STAB" }, { "SQUARE", "SAW", "SOFT" },
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
