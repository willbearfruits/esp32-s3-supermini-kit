#include "fx.h"
#include "sdkconfig.h"
#if defined(CONFIG_KIT_APP_LOOPER)
#include "looper.h"
const fx_t *const fx_list[] = { &fx_looper };
#elif defined(CONFIG_KIT_APP_TEST)
const fx_t *const fx_list[] = { &fx_sine, &fx_delay, &fx_reverb, &fx_stutter };
#elif defined(CONFIG_KIT_APP_JAM)
#include "jam.h"
const fx_t *const fx_list[] = { &fx_jam };
#elif defined(CONFIG_KIT_APP_FAUST)
const fx_t *const fx_list[] = { &fx_sine, &fx_faust_zita, &fx_faust_shift, &fx_faust_wah, &fx_faust_amp,
                                &fx_faust_flanger, &fx_faust_dyn, &fx_faust_synth };
#else
const fx_t *const fx_list[] = {
    &fx_vocoder, &fx_autotune, &fx_guitar_lead, &fx_guitar_chords,
    &fx_delay, &fx_reverb, &fx_stutter, &fx_test,
};
#endif
const int fx_count = sizeof fx_list / sizeof fx_list[0];
