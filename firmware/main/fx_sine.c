// Hardware test tone: 440 Hz sine, level sweeping -50 dB .. 0 dB .. -50 dB
// over 8 s, forever, with the mic passed straight through on top.
// For checking a DAC or amp on its own, then the mic with headphones.
#include "fx.h"
#include "dsp.h"
#include <stdio.h>

#define SWEEP_S 8.0f
#define TONE_HZ 440.0f
static float ph, t, db;
static volatile bool tone_on;
void fx_sine_set_tone(bool on) { tone_on = on; }

static void init(void) { ph = 0; t = 0; db = -50; }

static void process(const float *in, float *out, int n, const voice_t *v)
{
    const float inc = TONE_HZ / FS, dt = 1.0f / FS;
    for (int i = 0; i < n; i++) {
        float x = t / SWEEP_S;                          // 0..1
        float tri = x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x; // 0..1..0
        db = -50.0f + 50.0f * tri;
        float amp = powf(10.0f, db / 20.0f);
        out[i] = (tone_on ? fast_sin01(ph) * amp * 0.5f : 0.0f) + in[i];   // mic straight through, tone optional
        ph += inc; if (ph >= 1) ph -= 1;
        t += dt; if (t >= SWEEP_S) t -= SWEEP_S;
    }
}

static void status(char *buf, size_t len) { snprintf(buf, len, "440 Hz  %5.1f dB", db); }
float fx_sine_db(void) { return db; }

const fx_t fx_sine = { "SINE TEST", init, process, status, NULL, NULL, false };
