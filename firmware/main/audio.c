// Audio engine: one I2S port in full duplex, mic in, both DACs out. Runs the
// active mode's process() per block on core 1.
#include "audio.h"
#include "fx.h"
#include "dsp.h"
#include "pins.h"
#include "usb_midi.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "audio";

#define FRAMES 128
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
static biquad_t hp_in;

static void i2s_setup(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    chan_cfg.dma_frame_num = FRAMES;
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
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));
}

static void audio_task(void *arg)
{
    int32_t *raw = malloc(SAMPLES * sizeof(int32_t));
    float *in = malloc(FRAMES * sizeof(float));
    float *out = malloc(FRAMES * sizeof(float));
    assert(raw && in && out);

    biquad_highpass(&hp_in, 80.0f, 0.707f);
    voice_init();
    fx_list[mode_cur]->init();
    const float in_gain = (float)CONFIG_KIT_MIC_GAIN / 8388608.0f;   // 24-bit -> float

    while (1) {
        size_t got = 0;
        ESP_ERROR_CHECK(i2s_channel_read(rx, raw, SAMPLES * sizeof(int32_t), &got, portMAX_DELAY));
        int frames = got / (2 * sizeof(int32_t));

        // INMP441: left slot, top 24 bits are data, low 8 are junk
        for (int f = 0; f < frames; f++) {
            float x = (float)(raw[2 * f] >> 8) * in_gain;
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
        usb_midi_track_voice(&v);

        fx_list[mode_cur]->process(in, out, frames, &v);

        float pk = 0;
        for (int f = 0; f < frames; f++) {
            float y = softclip(out[f]) * 0.9f;
            float a = fabsf(y);
            if (a > pk) pk = a;
            int32_t s = (int32_t)(y * 2147483000.0f);
            raw[2 * f] = s;
            raw[2 * f + 1] = s;
            wave[f * AUDIO_WAVE_N / frames] = (int8_t)(y * 120.0f);
        }
        if (pk > out_peak) out_peak = pk;

#if AMP_ALWAYS_ON
        gpio_set_level(PIN_AMP_SD, 1);
#else
        gpio_set_level(PIN_AMP_SD, gpio_get_level(PIN_ENC_SW) == 0);
#endif
        size_t put = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx, raw, got, &put, portMAX_DELAY));
    }
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
