// Audio engine: one I2S port in full duplex, mic in, both DACs out. Runs the
// active mode's process() per block on core 1.
#include "audio.h"
#include "fx.h"
#include "dsp.h"
#include "pins.h"
#ifdef CONFIG_KIT_APP_LOOPER
#include "usbmode.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_cpu.h"
#include "esp_attr.h"
#include "sdkconfig.h"

static const char *TAG = "audio";

#define FRAMES 64                 // 2 ms blocks at 32 kHz
#define SAMPLES (FRAMES * 2)

#ifdef CONFIG_KIT_AMP_ALWAYS_ON
#define AMP_ALWAYS_ON 1
#else
#define AMP_ALWAYS_ON 0
#endif

static i2s_chan_handle_t tx, rx;
static volatile int mode_cur = 0, mode_req = 0;
static voice_t voice_now;
static int8_t wave[AUDIO_WAVE_N];
static volatile float out_peak;
static volatile float cpu_load;      // fraction of the block budget used, smoothed
static volatile float mic_gain = 8;
static volatile int32_t raw_l, raw_r;            // last frame's raw I2S words, for the mic test
static volatile int raw_nz_l, raw_nz_r;          // non-zero words in the last block
static volatile int raw_live_l, raw_live_r;      // words that are neither all-0 nor all-1 in the top 24 bits
static volatile int mic_slot;                    // 0 left, 1 right: the INMP441's L/R pin decides, found at runtime
static int slot_vote;
static volatile int raw_min, raw_max, raw_dc;    // of (word >> 8) in the last block
static biquad_t hp_in;

static i2s_std_gpio_config_t gpio_cfg_saved;

// Is an I2S pin held by the wiring? Weak test only: a pull-up alone must
// lift a free line, a pull-down alone must drop it. Never drives the pin
// hard, so a real short cannot pull the supply down.
// Returns 0 = pin free, 1 = held LOW, 2 = held HIGH.
int audio_pin_held(int pin)
{
    i2s_channel_disable(rx);
    i2s_channel_disable(tx);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY); esp_rom_delay_us(300);
    int up = gpio_get_level(pin);
    gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY); esp_rom_delay_us(300);
    int down = gpio_get_level(pin);
    gpio_reset_pin(pin);
    i2s_channel_reconfig_std_gpio(tx, &gpio_cfg_saved);
    i2s_channel_reconfig_std_gpio(rx, &gpio_cfg_saved);
    i2s_channel_enable(tx);
    i2s_channel_enable(rx);
    ESP_LOGI(TAG, "pin %d: with pull-up reads %d, with pull-down reads %d", pin, up, down);
    if (up == 0 && down == 0) return 1;
    if (up == 1 && down == 1) return 2;
    return 0;
}

static void i2s_setup(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    chan_cfg.dma_frame_num = FRAMES;
    chan_cfg.dma_desc_num = 3;        // shallow queue: ~6 ms out, not ~48
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx, &rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_KIT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din  = PIN_I2S_DIN,
        },
    };
    gpio_cfg_saved = std_cfg.gpio_cfg;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));
}

static void IRAM_ATTR audio_task(void *arg)
{
    int32_t *raw = malloc(SAMPLES * sizeof(int32_t));
    float *in = malloc(FRAMES * sizeof(float));
    float *out = malloc(2 * FRAMES * sizeof(float));
    assert(raw && in && out);

    biquad_highpass(&hp_in, 80.0f, 0.707f);
    voice_init();
    fx_list[mode_cur]->init();
    mic_gain = CONFIG_KIT_MIC_GAIN;

    while (1) {
        size_t got = 0;
        ESP_ERROR_CHECK(i2s_channel_read(rx, raw, SAMPLES * sizeof(int32_t), &got, portMAX_DELAY));
        int frames = got / (2 * sizeof(int32_t));
        uint32_t c0 = esp_cpu_get_cycle_count();

        // INMP441: top 24 bits are data, low 8 are junk. It drives one slot
        // (L/R pin low = left) and tri-states the other, which then reads as
        // all ones or all zeros; whichever slot is alive is the mic.
        { int nl = 0, nr = 0, ll = 0, lr = 0, mn = 0x7FFFFFFF, mx = -0x7FFFFFFF; long long acc = 0;
          for (int f = 0; f < frames; f++) {
              int wl = raw[2 * f] >> 8, wr = raw[2 * f + 1] >> 8;
              nl += raw[2 * f] != 0; nr += raw[2 * f + 1] != 0;
              ll += wl != 0 && wl != -1; lr += wr != 0 && wr != -1;
              int v = raw[2 * f + mic_slot] >> 8; if (v < mn) mn = v; if (v > mx) mx = v; acc += v;
          }
          raw_nz_l = nl; raw_nz_r = nr; raw_live_l = ll; raw_live_r = lr;
          raw_l = raw[2 * (frames - 1)]; raw_r = raw[2 * (frames - 1) + 1];
          raw_min = mn; raw_max = mx; raw_dc = (int)(acc / frames);
          int want = (ll < frames / 4 && lr > frames / 2) ? 1 : (lr < frames / 4 && ll > frames / 2) ? 0 : mic_slot;
          if (want != mic_slot) { if (++slot_vote > 50) { mic_slot = want; slot_vote = 0;
              ESP_LOGW(TAG, "mic data is in the %s slot (L/R pin %s), using it", want ? "RIGHT" : "LEFT", want ? "high" : "low"); } }
          else slot_vote = 0; }
        for (int f = 0; f < frames; f++) {
            float x = (float)(raw[2 * f + mic_slot] >> 8) * (mic_gain / 8388608.0f);
            in[f] = biquad_run(&hp_in, x);
        }

        if (mode_req != mode_cur) {
            mode_cur = mode_req;
            fx_list[mode_cur]->init();
            ESP_LOGI(TAG, "mode %d: %s", mode_cur, fx_list[mode_cur]->name);
        }

        voice_t v;
        voice_feed(in, frames, &v);
        voice_now = v;
#ifdef CONFIG_KIT_APP_LOOPER
        usbmode_track_voice(&v);
#endif

        const fx_t *fx = fx_list[mode_cur];
        fx->process(in, out, frames, &v);

        float pk = 0;
        for (int f = 0; f < frames; f++) {
            float l, r;
            if (fx->stereo) { l = clampf(out[2 * f], -1, 1) * 0.95f; r = clampf(out[2 * f + 1], -1, 1) * 0.95f; }
            else l = r = softclip(out[f]) * 0.9f;
            float a = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
            if (a > pk) pk = a;
            raw[2 * f] = (int32_t)(l * 2147483000.0f);
            raw[2 * f + 1] = (int32_t)(r * 2147483000.0f);
            wave[f * AUDIO_WAVE_N / frames] = (int8_t)((l + r) * 60.0f);
        }
        if (pk > out_peak) out_peak = pk;
        float used = (float)(esp_cpu_get_cycle_count() - c0) / (frames * (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1e6f / CONFIG_KIT_SAMPLE_RATE));
        cpu_load += 0.02f * (used - cpu_load);

#if AMP_ALWAYS_ON
        gpio_set_level(PIN_AMP_SD, 1);
#else
        gpio_set_level(PIN_AMP_SD, gpio_get_level(PIN_ENC_SW) == 0);
#endif
        size_t put = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx, raw, got, &put, portMAX_DELAY));
    }
}

// Stop and restart the I2S clocks. An INMP441 that lost frame sync (data
// stuck at all ones or zeros) resynchronises on the next WS edge.
void audio_restart_i2s(void)
{
    i2s_channel_disable(rx);
    i2s_channel_disable(tx);
    vTaskDelay(pdMS_TO_TICKS(20));
    i2s_channel_enable(tx);
    i2s_channel_enable(rx);
}

void audio_start(void)
{
    i2s_setup();
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 6, NULL, 1);
}

void audio_set_mode(int idx)
{
    if (idx < 0) idx = fx_count - 1;
    if (idx >= fx_count) idx = 0;
    mode_req = idx;
}

int audio_get_mode(void) { return mode_cur; }
void audio_get_voice(voice_t *o) { *o = voice_now; }
void audio_get_wave(int8_t *o) { memcpy(o, wave, AUDIO_WAVE_N); }
float audio_out_peak(void) { float p = out_peak; out_peak = 0; return p; }
float audio_cpu_load(void) { return cpu_load; }
void  audio_set_mic_gain(float g) { mic_gain = g; }
float audio_get_mic_gain(void) { return mic_gain; }
void  audio_get_raw(int32_t *l, int32_t *r, int *nz_l, int *nz_r) { *l = raw_l; *r = raw_r; *nz_l = raw_nz_l; *nz_r = raw_nz_r; }
void  audio_get_raw_live(int *live_l, int *live_r) { *live_l = raw_live_l; *live_r = raw_live_r; }
int   audio_mic_slot(void) { return mic_slot; }
void  audio_get_raw_stats(int *mn, int *mx, int *dc) { *mn = raw_min; *mx = raw_max; *dc = raw_dc; }

void audio_midi_note_on(int note, int vel)
{
    const fx_t *f = fx_list[mode_cur];
    if (f->note_on) f->note_on(note, vel);
}

void audio_midi_note_off(int note)
{
    const fx_t *f = fx_list[mode_cur];
    if (f->note_off) f->note_off(note);
}
