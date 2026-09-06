#include "selftest.h"
#include "pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "selftest";

void selftest_pins(void)
{
    // Drive each I2S pin and see whether any other one follows: shorted wires.
    const int pins[] = { PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_AMP_SD };
    const int n = sizeof pins / sizeof pins[0];
    int shorts = 0;
    for (int d = 0; d < n; d++) {
        for (int y = d + 1; y < n; y++) {
            gpio_reset_pin(pins[y]);
            gpio_set_direction(pins[y], GPIO_MODE_INPUT);
            gpio_reset_pin(pins[d]);
            gpio_set_direction(pins[d], GPIO_MODE_OUTPUT);
            gpio_set_pull_mode(pins[y], GPIO_PULLDOWN_ONLY);
            gpio_set_level(pins[d], 1);
            vTaskDelay(1);
            int hi = gpio_get_level(pins[y]) == 1;
            gpio_set_pull_mode(pins[y], GPIO_PULLUP_ONLY);
            gpio_set_level(pins[d], 0);
            vTaskDelay(1);
            int lo = gpio_get_level(pins[y]) == 0;
            if (hi && lo) {
                ESP_LOGE(TAG, "GPIO%d and GPIO%d are SHORTED together", pins[d], pins[y]);
                shorts++;
            }
        }
    }
    for (int i = 0; i < n; i++) gpio_reset_pin(pins[i]);
    if (!shorts) ESP_LOGI(TAG, "no shorts between I2S pins");

    // Mic data line: floating or held by something?
    gpio_set_direction(PIN_I2S_DIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLDOWN_ONLY);
    vTaskDelay(1);
    int down = gpio_get_level(PIN_I2S_DIN);
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLUP_ONLY);
    vTaskDelay(1);
    int up = gpio_get_level(PIN_I2S_DIN);
    gpio_reset_pin(PIN_I2S_DIN);
    if (down == 0 && up == 1) ESP_LOGI(TAG, "mic SD line GPIO%d idle (high-Z), as expected", PIN_I2S_DIN);
    else ESP_LOGW(TAG, "mic SD line GPIO%d is held %s by something", PIN_I2S_DIN, up ? "HIGH" : "LOW");
}
