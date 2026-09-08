#include "fx.h"
#include "sdkconfig.h"
#if defined(CONFIG_KIT_APP_LOOPER)
#include "looper.h"
const fx_t *const fx_list[] = { &fx_looper };
#elif defined(CONFIG_KIT_APP_TEST)
const fx_t *const fx_list[] = { &fx_sine };
#else
const fx_t *const fx_list[] = {
    &fx_vocoder, &fx_autotune, &fx_guitar_lead, &fx_guitar_chords,
    &fx_delay, &fx_reverb, &fx_stutter, &fx_test,
};
#endif
const int fx_count = sizeof fx_list / sizeof fx_list[0];
