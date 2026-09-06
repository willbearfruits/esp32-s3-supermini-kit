// Milestone 0: prove the shared I2S bus.
//
// One I2S port in full duplex: the INMP441 mic feeds DIN, the PCM5102A and
// MAX98357A both listen on DOUT, and all three share BCLK/WS. Audio from the
// mic is gained, duplicated to both channels, and written straight back out.
// A peak meter is printed once a second so you can verify the mic even with
// the speaker muted.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "pins.h"

static const char *TAG = "loopback";

#ifdef CONFIG_KIT_AMP_ALWAYS_ON
#define AMP_ALWAYS_ON 1
#else
#define AMP_ALWAYS_ON 0
#endif

#define FRAMES_PER_BLOCK 256                     // stereo frames per read/write
#define SAMPLES_PER_BLOCK (FRAMES_PER_BLOCK * 2) // L + R
#define BLOCK_BYTES (SAMPLES_PER_BLOCK * sizeof(int32_t))

static void gpio_setup(void)
{
    gpio_config_t amp = {
        .pin_bit_mask = 1ULL << PIN_AMP_SD,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&amp));
    gpio_set_level(PIN_AMP_SD, 0); // start muted

    gpio_config_t sw = {
        .pin_bit_mask = 1ULL << PIN_ENC_SW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sw));
}

static void i2s_setup(i2s_chan_handle_t *tx, i2s_chan_handle_t *rx)
{
    // Creating TX and RX in the same call on the same port gives full duplex:
    // both channels share BCLK and WS, so they must use identical clock config.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // send silence instead of stale data on underrun
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx, rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_KIT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // none of the three chips needs MCLK
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din  = PIN_I2S_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx));
}

static inline int32_t clamp32(int64_t v)
{
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return (int32_t)v;
}

void app_main(void)
{
    ESP_LOGI(TAG, "kit loopback: %d Hz, gain x%d, amp %s",
             CONFIG_KIT_SAMPLE_RATE, CONFIG_KIT_MIC_GAIN,
             AMP_ALWAYS_ON ? "always on" : "on while encoder button held");

    gpio_setup();

    i2s_chan_handle_t tx = NULL, rx = NULL;
    i2s_setup(&tx, &rx);

    int32_t *buf = malloc(BLOCK_BYTES);
    assert(buf);

    int32_t peak = 0;
    int64_t last_print = esp_timer_get_time();

    while (1) {
        size_t got = 0;
        ESP_ERROR_CHECK(i2s_channel_read(rx, buf, BLOCK_BYTES, &got, portMAX_DELAY));
        size_t samples = got / sizeof(int32_t);

        // INMP441 with L/R tied to GND puts its data in the LEFT slot.
        // Copy left to right so both DAC channels (and the amp's L+R mix) get it.
        for (size_t i = 0; i + 1 < samples; i += 2) {
            int32_t s = clamp32((int64_t)buf[i] * CONFIG_KIT_MIC_GAIN);
            buf[i] = s;
            buf[i + 1] = s;
            int32_t a = s < 0 ? -s : s;
            if (a > peak) peak = a;
        }

#if AMP_ALWAYS_ON
        gpio_set_level(PIN_AMP_SD, 1);
#else
        gpio_set_level(PIN_AMP_SD, gpio_get_level(PIN_ENC_SW) == 0);
#endif

        size_t put = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx, buf, got, &put, portMAX_DELAY));

        int64_t now = esp_timer_get_time();
        if (now - last_print > 1000000) {
            float dbfs = peak > 0 ? 20.0f * log10f((float)peak / 2147483648.0f) : -120.0f;
            int bars = (int)((dbfs + 60.0f) / 2.0f);
            if (bars < 0) bars = 0;
            if (bars > 30) bars = 30;
            char meter[32];
            memset(meter, '#', bars);
            meter[bars] = '\0';
            ESP_LOGI(TAG, "peak %6.1f dBFS |%-30s|", dbfs, meter);
            peak = 0;
            last_print = now;
        }
    }
}
