#include "dsp.h"
#include <stdio.h>

static void rbj(biquad_t *q, float b0, float b1, float b2, float a0, float a1, float a2)
{
    q->b0 = b0 / a0; q->b1 = b1 / a0; q->b2 = b2 / a0;
    q->a1 = a1 / a0; q->a2 = a2 / a0;
    q->z1 = q->z2 = 0;
}

void biquad_lowpass(biquad_t *q, float f0, float Q)
{
    float w = TWO_PI * f0 / FS, c = cosf(w), a = sinf(w) / (2 * Q);
    rbj(q, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
}

void biquad_highpass(biquad_t *q, float f0, float Q)
{
    float w = TWO_PI * f0 / FS, c = cosf(w), a = sinf(w) / (2 * Q);
    rbj(q, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
}

void biquad_bandpass(biquad_t *q, float f0, float Q)
{
    float w = TWO_PI * f0 / FS, c = cosf(w), a = sinf(w) / (2 * Q);
    rbj(q, a, 0, -a, 1 + a, -2 * c, 1 - a); // constant 0 dB peak gain
}

const char *note_name(int midi, char *buf)
{
    static const char *names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (midi < 0 || midi > 127) { buf[0] = '-'; buf[1] = 0; return buf; }
    snprintf(buf, 5, "%s%d", names[midi % 12], midi / 12 - 1);
    return buf;
}
