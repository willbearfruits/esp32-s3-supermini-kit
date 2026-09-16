#include "faust_voice.h"
#include "faust_glue.h"
extern "C" {
#include "sdkconfig.h"
#include "esp_log.h"
}
#include "faust/jam_voice.h"

struct fvoice {
    kfx_jam_voice *d; faust_param_ui ui;
    FAUSTFLOAT *freq, *gate, *vel, *bright;
};

struct setting { const char *label; float value; };
struct preset { const char *name; setting set[12]; };
static const preset PRE[FV_KIND_N][FV_PRESETS] = {
    { // bass
      { "SUB",   { { "wave", 1 }, { "sub", 0.6f }, { "cutoff", 300 },  { "res", 0.1f },  { "envf", 0.2f }, { "att", 0.004f }, { "dec", 0.25f }, { "sus", 0.8f }, { "rel", 0.12f }, { "drive", 0.1f }, { "gain", 0.55f }, { NULL, 0 } } },
      { "ACID",  { { "wave", 0 }, { "sub", 0 },    { "cutoff", 250 },  { "res", 0.85f }, { "envf", 0.9f }, { "att", 0.002f }, { "dec", 0.18f }, { "sus", 0.1f }, { "rel", 0.1f },  { "drive", 0.4f }, { "gain", 0.4f },  { NULL, 0 } } },
      { "REESE", { { "wave", 0 }, { "sub", 0.4f }, { "cutoff", 600 },  { "res", 0.3f },  { "envf", 0.3f }, { "att", 0.01f },  { "dec", 0.4f },  { "sus", 0.9f }, { "rel", 0.2f },  { "drive", 0.6f }, { "gain", 0.4f },  { NULL, 0 } } },
      { "PLUCK", { { "wave", 1 }, { "sub", 0.3f }, { "cutoff", 400 },  { "res", 0.5f },  { "envf", 1.0f }, { "att", 0.002f }, { "dec", 0.12f }, { "sus", 0.0f }, { "rel", 0.1f },  { "drive", 0.2f }, { "gain", 0.5f },  { NULL, 0 } } },
      { "WOBBLE",{ { "wave", 1 }, { "sub", 0.5f }, { "cutoff", 200 },  { "res", 0.7f },  { "envf", 0.6f }, { "att", 0.005f }, { "dec", 0.6f },  { "sus", 0.6f }, { "rel", 0.2f },  { "drive", 0.5f }, { "gain", 0.45f }, { NULL, 0 } } },
    },
    { // lead
      { "SAW",   { { "wave", 0 }, { "sub", 0 },    { "cutoff", 1800, }, { "res", 0.2f }, { "envf", 0.5f }, { "att", 0.005f }, { "dec", 0.3f }, { "sus", 0.7f }, { "rel", 0.25f }, { "drive", 0.1f }, { "gain", 0.35f }, { NULL, 0 } } },
      { "SQUARE",{ { "wave", 1 }, { "sub", 0 },    { "cutoff", 2500 },  { "res", 0.3f }, { "envf", 0.3f }, { "att", 0.01f },  { "dec", 0.2f }, { "sus", 0.8f }, { "rel", 0.2f },  { "drive", 0.0f }, { "gain", 0.3f },  { NULL, 0 } } },
      { "FLUTE", { { "wave", 2 }, { "sub", 0 },    { "cutoff", 3000 },  { "res", 0.1f }, { "envf", 0.2f }, { "att", 0.05f },  { "dec", 0.3f }, { "sus", 0.9f }, { "rel", 0.3f },  { "drive", 0.0f }, { "gain", 0.45f }, { NULL, 0 } } },
      { "PULSE", { { "wave", 1 }, { "sub", 0.2f }, { "cutoff", 1200 },  { "res", 0.6f }, { "envf", 0.8f }, { "att", 0.003f }, { "dec", 0.25f }, { "sus", 0.3f }, { "rel", 0.2f },  { "drive", 0.3f }, { "gain", 0.35f }, { NULL, 0 } } },
      { "SCREAM",{ { "wave", 0 }, { "sub", 0.3f }, { "cutoff", 900 },   { "res", 0.9f }, { "envf", 1.0f }, { "att", 0.002f }, { "dec", 0.5f }, { "sus", 0.6f }, { "rel", 0.3f },  { "drive", 0.8f }, { "gain", 0.3f },  { NULL, 0 } } },
    },
};

static void set(fvoice *v, const char *label, float x) { FAUSTFLOAT *z = v->ui.find(label); if (z) *z = x; }

fvoice_t *fvoice_new(int kind, int preset)
{
    fvoice *v = new fvoice();
    v->d = faust_make<kfx_jam_voice>(NULL);
    v->d->init(CONFIG_KIT_SAMPLE_RATE);
    v->d->buildUserInterface(&v->ui);
    v->freq = v->ui.find("freq"); v->gate = v->ui.find("gate"); v->vel = v->ui.find("vel"); v->bright = v->ui.find("bright");
    fvoice_preset(v, kind, preset);
    return v;
}

void fvoice_preset(fvoice_t *v, int kind, int preset)
{
    if (kind < 0 || kind >= FV_KIND_N) kind = 0;
    if (preset < 0 || preset >= FV_PRESETS) preset = 0;
    const struct preset *p = &PRE[kind][preset];
    for (int i = 0; i < 12 && p->set[i].label; i++) set(v, p->set[i].label, p->set[i].value);
}

const char *fvoice_preset_name(int kind, int preset)
{
    if (kind < 0 || kind >= FV_KIND_N || preset < 0 || preset >= FV_PRESETS) return "?";
    return PRE[kind][preset].name;
}

void fvoice_note(fvoice_t *v, float freq, float vel, bool on)
{
    if (on) { *v->freq = freq; *v->vel = vel; *v->gate = 1; }
    else *v->gate = 0;
}

void fvoice_bright(fvoice_t *v, float mul) { *v->bright = mul; }

static float scratch[256];
void fvoice_render(fvoice_t *v, float *out, int n)
{
    if (n > 256) n = 256;
    FAUSTFLOAT *ins[1] = { NULL }; FAUSTFLOAT *outs[1] = { scratch };
    v->d->compute(n, ins, outs);
    for (int i = 0; i < n; i++) out[i] += scratch[i];
}
