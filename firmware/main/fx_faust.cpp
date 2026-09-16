// Faust effects as fx_t modes. Each Faust program in ../faust/*.dsp is
// compiled by tools/faustgen.sh into faust/<name>.h (a class kfx_<name>).
// This file instantiates them, collects their sliders by label, drives
// "freq" and "gate" from the voice tracker when a program has them, and
// exposes named presets that set sliders. Big objects (long delay lines)
// are placed in PSRAM.
extern "C" {
#include "fx.h"
#include "audio.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
}
#include <new>
#include <cstring>
#include <cstdio>
#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"
#include "faust/zita.h"
#include "faust/shift.h"
#include "faust/wah.h"
#include "faust/amp.h"
#include "faust/flanger.h"
#include "faust/dyn.h"
#include "faust/synth.h"

static const char *TAG = "faust";
#define MAXP 24
#define MAXN 256                 // largest block the audio engine hands us
#define PSRAM_FROM 40000         // objects bigger than this go to PSRAM

struct setting { const char *label; float value; };
struct preset { const char *name; setting set[8]; };

// Collects every slider/button/nentry as (label, zone).
struct param_ui : public UI {
    const char *label[MAXP]; FAUSTFLOAT *zone[MAXP]; int n = 0;
    void add(const char *l, FAUSTFLOAT *z) { if (n < MAXP) { label[n] = l; zone[n] = z; n++; } }
    void openTabBox(const char *) override {}
    void openHorizontalBox(const char *) override {}
    void openVerticalBox(const char *) override {}
    void closeBox() override {}
    void addButton(const char *l, FAUSTFLOAT *z) override { add(l, z); }
    void addCheckButton(const char *l, FAUSTFLOAT *z) override { add(l, z); }
    void addHorizontalSlider(const char *l, FAUSTFLOAT *z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { add(l, z); }
    void addVerticalSlider(const char *l, FAUSTFLOAT *z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { add(l, z); }
    void addNumEntry(const char *l, FAUSTFLOAT *z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { add(l, z); }
    void addHorizontalBargraph(const char *, FAUSTFLOAT *, FAUSTFLOAT, FAUSTFLOAT) override {}
    void addVerticalBargraph(const char *, FAUSTFLOAT *, FAUSTFLOAT, FAUSTFLOAT) override {}
    void addSoundfile(const char *, const char *, Soundfile **) override {}
    FAUSTFLOAT *find(const char *l) { for (int i = 0; i < n; i++) if (!strcmp(label[i], l)) return zone[i]; return NULL; }
};

struct instance {
    dsp *d = NULL; int nin = 0, nout = 0; size_t bytes = 0; bool psram = false;
    param_ui ui; FAUSTFLOAT *freq = NULL, *gate = NULL;
    const preset *pre; int npre, pcur = 0;
    const char *name;
};

static float scratch[2][MAXN];

static void apply(instance *h)
{
    const preset *p = &h->pre[h->pcur];
    for (int i = 0; i < 8 && p->set[i].label; i++) {
        FAUSTFLOAT *z = h->ui.find(p->set[i].label);
        if (z) *z = p->set[i].value;
        else ESP_LOGW(TAG, "%s: no slider '%s'", h->name, p->set[i].label);
    }
}

template <class T> static void make(instance *h)
{
    if (!h->d) {
        h->bytes = sizeof(T);
        h->psram = h->bytes > PSRAM_FROM;
        void *mem = h->psram ? heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM) : malloc(sizeof(T));
        if (!mem) { mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM); h->psram = true; }
        h->d = new (mem) T();
        h->d->init(CONFIG_KIT_SAMPLE_RATE);
        h->d->buildUserInterface(&h->ui);
        h->nin = h->d->getNumInputs(); h->nout = h->d->getNumOutputs();
        h->freq = h->ui.find("freq"); h->gate = h->ui.find("gate");
        ESP_LOGI(TAG, "%s: %u bytes in %s, %d in %d out, %d params", h->name, (unsigned)h->bytes,
                 h->psram ? "PSRAM" : "internal RAM", h->nin, h->nout, h->ui.n);
    } else {
        h->d->instanceClear();
    }
    apply(h);
}

static void run(instance *h, const float *in, float *out, int n, const voice_t *v)
{
    if (n > MAXN) n = MAXN;
    if (h->freq && v->voiced && v->freq > 20) *h->freq = v->freq;
    if (h->gate) *h->gate = v->gate ? 1.0f : 0.0f;
    FAUSTFLOAT *ins[2] = { (FAUSTFLOAT *)in, (FAUSTFLOAT *)in };
    if (h->nout == 2) {
        FAUSTFLOAT *outs[2] = { scratch[0], scratch[1] };
        h->d->compute(n, ins, outs);
        for (int i = 0; i < n; i++) { out[2 * i] = scratch[0][i]; out[2 * i + 1] = scratch[1][i]; }
    } else {
        FAUSTFLOAT *outs[1] = { out };
        h->d->compute(n, ins, outs);
    }
}

static const char *select_preset(instance *h, int idx)
{
    if (idx < 0 || idx >= h->npre) return NULL;
    h->pcur = idx; apply(h);
    return h->pre[idx].name;
}

#define FAUST_FX(ident, Class, NAME, STEREO, ...)                                                   \
    static const preset ident##_pre[] = __VA_ARGS__;                                                \
    static instance ident##_h = { NULL, 0, 0, 0, false, {}, NULL, NULL, ident##_pre,                \
                                  (int)(sizeof ident##_pre / sizeof ident##_pre[0]), 0, NAME };     \
    static void ident##_init(void) { make<Class>(&ident##_h); }                                     \
    static void ident##_process(const float *in, float *out, int n, const voice_t *v) { run(&ident##_h, in, out, n, v); } \
    static void ident##_status(char *b, size_t l) { snprintf(b, l, "%s%s", ident##_h.pre[ident##_h.pcur].name, ident##_h.psram ? " (psram)" : ""); } \
    static const char *ident##_preset(int i) { return select_preset(&ident##_h, i); }               \
    extern "C" const fx_t ident = { NAME, ident##_init, ident##_process, ident##_status, NULL, NULL, STEREO, ident##_preset };

FAUST_FX(fx_faust_zita, kfx_zita, "ZITA REVERB", true, {
    { "room",      { { "t60", 1.2f }, { "wet", 0.35f }, { "predelay", 15 }, { "damp", 4000 }, { NULL, 0 } } },
    { "hall",      { { "t60", 3.0f }, { "wet", 0.45f }, { "predelay", 30 }, { "damp", 5000 }, { NULL, 0 } } },
    { "cathedral", { { "t60", 8.0f }, { "wet", 0.55f }, { "predelay", 60 }, { "damp", 3000 }, { NULL, 0 } } },
    { "infinite",  { { "t60", 12.0f }, { "wet", 0.8f }, { "predelay", 40 }, { "damp", 8000 }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_shift, kfx_shift, "PITCH SHIFT", false, {
    { "octave up",    { { "semi", 12 },  { "mix", 1.0f }, { NULL, 0 } } },
    { "fifth harmony",{ { "semi", 7 },   { "mix", 0.5f }, { NULL, 0 } } },
    { "octave down",  { { "semi", -12 }, { "mix", 0.7f }, { NULL, 0 } } },
    { "demon",        { { "semi", -19 }, { "mix", 1.0f }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_wah, kfx_wah, "WAH", false, {
    { "auto-wah",   { { "mode", 0 }, { "sens", 6 },  { NULL, 0 } } },
    { "touchy",     { { "mode", 0 }, { "sens", 20 }, { NULL, 0 } } },
    { "slow lfo",   { { "mode", 1 }, { "rate", 0.3f }, { "depth", 1 }, { NULL, 0 } } },
    { "fast lfo",   { { "mode", 1 }, { "rate", 4 },    { "depth", 0.8f }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_amp, kfx_amp, "AMP SIM", false, {
    { "clean boost", { { "pre", 2 },  { "drive", 0.1f }, { "lo", 80 },  { "hi", 8000 }, { "post", 0.7f }, { NULL, 0 } } },
    { "crunch",      { { "pre", 6 },  { "drive", 0.5f }, { "lo", 120 }, { "hi", 5000 }, { "post", 0.5f }, { NULL, 0 } } },
    { "fuzz",        { { "pre", 30 }, { "drive", 0.9f }, { "lo", 150 }, { "hi", 4000 }, { "post", 0.3f }, { NULL, 0 } } },
    { "telephone",   { { "pre", 8 },  { "drive", 0.6f }, { "lo", 600 }, { "hi", 2500 }, { "post", 0.5f }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_flanger, kfx_flanger, "FLANGER", false, {
    { "slow flange", { { "rate", 0.2f }, { "depth", 5 },  { "base", 0.5f }, { "fb", 0.6f },  { "mix", 0.5f }, { NULL, 0 } } },
    { "jet",         { { "rate", 0.6f }, { "depth", 8 },  { "base", 0.2f }, { "fb", 0.9f },  { "mix", 0.5f }, { NULL, 0 } } },
    { "chorus",      { { "rate", 1.2f }, { "depth", 4 },  { "base", 15 },   { "fb", 0.0f },  { "mix", 0.5f }, { NULL, 0 } } },
    { "vibrato",     { { "rate", 5.0f }, { "depth", 3 },  { "base", 5 },    { "fb", 0.0f },  { "mix", 1.0f }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_dyn, kfx_dyn, "COMPRESSOR", false, {
    { "gentle",  { { "ratio", 2 },  { "thresh", -24 }, { "att", 0.01f },  { "rel", 0.2f },  { "gain", 6 },  { NULL, 0 } } },
    { "squash",  { { "ratio", 10 }, { "thresh", -40 }, { "att", 0.002f }, { "rel", 0.15f }, { "gain", 20 }, { NULL, 0 } } },
    { "pump",    { { "ratio", 20 }, { "thresh", -30 }, { "att", 0.001f }, { "rel", 0.8f },  { "gain", 18 }, { NULL, 0 } } },
    { "limiter", { { "ratio", 1 },  { "thresh", 0 },   { "att", 0.005f }, { "rel", 0.15f }, { "gain", 24 }, { NULL, 0 } } },
})

FAUST_FX(fx_faust_synth, kfx_synth, "SYNTH FOLLOW", false, {
    { "saw moog",  { { "wave", 0 }, { "octave", 0 },  { "cutoff", 1500 }, { "res", 0.3f }, { "mic", 0.3f }, { NULL, 0 } } },
    { "sub bass",  { { "wave", 1 }, { "octave", -1 }, { "cutoff", 400 },  { "res", 0.2f }, { "mic", 0.2f }, { NULL, 0 } } },
    { "acid",      { { "wave", 0 }, { "octave", -1 }, { "cutoff", 2500 }, { "res", 0.85f }, { "mic", 0.0f }, { NULL, 0 } } },
    { "flute",     { { "wave", 2 }, { "octave", 1 },  { "cutoff", 3000 }, { "res", 0.1f }, { "mic", 0.4f }, { NULL, 0 } } },
})
