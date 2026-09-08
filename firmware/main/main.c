// Common bring-up: self-test, GPIO, I2C scan, OLED, audio engine. Then hands
// over to the application chosen in menuconfig (Kit firmware -> Application):
// the voice looper or the multi-mode voice instrument.
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "pins.h"
#include "oled.h"
#include "audio.h"
#include "fx.h"
#include "selftest.h"
#include "song.h"
#include "app.h"

static const char *TAG = "kit";
#define OLED_ADDR 0x3C

static void gpio_setup(void)
{
    gpio_config_t amp = { .pin_bit_mask = 1ULL << PIN_AMP_SD, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&amp));
    gpio_set_level(PIN_AMP_SD, 0);

    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_ENC_SW, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&in));
}

static i2c_master_bus_handle_t i2c_setup(void)
{
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) ESP_LOGI(TAG, "I2C device at 0x%02X", a);
    }
    return bus;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(300));
#if defined(CONFIG_KIT_APP_LOOPER)
    ESP_LOGI(TAG, "kit voice looper, %d Hz", CONFIG_KIT_SAMPLE_RATE);
#elif defined(CONFIG_KIT_APP_TEST)
    ESP_LOGI(TAG, "kit hardware test, %d Hz", CONFIG_KIT_SAMPLE_RATE);
#else
    ESP_LOGI(TAG, "kit voice instrument, %d modes, %d Hz", fx_count, CONFIG_KIT_SAMPLE_RATE);
#endif

    selftest_pins();
    gpio_setup();

    i2c_master_bus_handle_t bus = i2c_setup();
    if (oled_init(bus, OLED_ADDR) == ESP_OK) ESP_LOGI(TAG, "OLED up");
    else ESP_LOGW(TAG, "no OLED, continuing without display");

#ifdef CONFIG_KIT_APP_LOOPER
    song_init();
#endif
    audio_start();

#if defined(CONFIG_KIT_APP_LOOPER)
    app_looper_run(bus);
#elif defined(CONFIG_KIT_APP_TEST)
    app_test_run(bus);
#else
    app_instrument_run();
#endif
}
