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

void IRAM_ATTR synth_render(synth_t *s, float *out, int n)
{
    const synth_cfg_t *c = &s->cfg;
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
            float dt = v->freq * inv_fs;
            float y = osc(v->ph1, dt, c->square) + osc(v->ph2, dt * det, c->square);
            if (c->sub > 0) y += c->sub * osc(v->phs, dt * 0.5f, true);
            v->ph1 += dt; if (v->ph1 >= 1) v->ph1 -= 1;
            v->ph2 += dt * det; if (v->ph2 >= 1) v->ph2 -= 1;
            v->phs += dt * 0.5f; if (v->phs >= 1) v->phs -= 1;
            svf_set(&v->lpf, clampf(fc_base + c->env_hz * v->env, 40.0f, SVF_MAX_HZ), c->q);
            out[k] += svf_run(&v->lpf, y, NULL, NULL) * v->env * g;
        }
    }
}
