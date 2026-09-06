#include "fx.h"
const fx_t *const fx_list[] = {
    &fx_vocoder, &fx_autotune, &fx_guitar_lead, &fx_guitar_chords,
    &fx_delay, &fx_reverb, &fx_stutter, &fx_test,
};
const int fx_count = sizeof fx_list / sizeof fx_list[0];
