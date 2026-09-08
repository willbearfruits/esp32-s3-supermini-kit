// Hardware test: plays the sine sweep from fx_sine.c and reports once a
// second. Works with nothing but a DAC wired; the OLED is used if present.
#include "app.h"
#include "audio.h"
#include "oled.h"
#include "dsp.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "test";
float fx_sine_db(void);

void app_test_run(void)
{
    ESP_LOGI(TAG, "sine 440 Hz, level sweeps -50..0 dB every 8 s, %d Hz, both DAC outputs", CONFIG_KIT_SAMPLE_RATE);
    int n = 0;
    while (1) {
        float db = fx_sine_db();
        int bar = (int)((db + 50) / 50 * 30);
        char line[40];
        snprintf(line, sizeof line, "|%-30.*s|", bar, "##############################");
        ESP_LOGI(TAG, "sine 440 Hz  %5.1f dB %s  peak %5.2f  cpu %2.0f%%", db, line, audio_out_peak(), audio_cpu_load() * 100);
        if (oled_present()) {
            oled_clear();
            oled_text2(4, 4, "SINE TEST");
            snprintf(line, sizeof line, "440 Hz  %5.1f dB", db);
            oled_text(4, 26, line);
            oled_rect(4, 40, 120, 8, false);
            oled_rect(5, 41, bar * 4 - 2 > 0 ? bar * 4 - 2 : 0, 6, true);
            oled_flush();
        }
        n++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
