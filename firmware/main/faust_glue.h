// Shared C++ glue for Faust-generated classes: a UI that collects sliders
// by label, and placement of big objects in PSRAM.
#pragma once
extern "C" {
#include "esp_heap_caps.h"
}
#include <new>
#include <cstring>
#include <cstdlib>
#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"

#define FAUST_MAXP 24
#define FAUST_PSRAM_FROM 40000

struct faust_param_ui : public UI {
    const char *label[FAUST_MAXP]; FAUSTFLOAT *zone[FAUST_MAXP]; int n = 0;
    void add(const char *l, FAUSTFLOAT *z) { if (n < FAUST_MAXP) { label[n] = l; zone[n] = z; n++; } }
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

// Construct T in internal RAM, or PSRAM when it is big (or internal is full).
template <class T> static T *faust_make(bool *in_psram)
{
    bool ps = sizeof(T) > FAUST_PSRAM_FROM;
    void *mem = ps ? heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM) : malloc(sizeof(T));
    if (!mem) { mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM); ps = true; }
    if (in_psram) *in_psram = ps;
    return mem ? new (mem) T() : NULL;
}
