#include "input.h"
#include "pins.h"
#include <string.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "input";
#define PIN_BOOT 0

static pcnt_unit_handle_t pcnt;
static adc_oneshot_unit_handle_t adc;
static bool has_joy;
static float jx0, jy0, jx_f, jy_f;
static int enc_acc;
static bool was_pressed, long_fired;
static int64_t t_press;
static bool flick_armed[4] = { true, true, true, true };

static int adc_read(adc_channel_t ch)
{
    int raw = 0;
    return adc_oneshot_read(adc, ch, &raw) == ESP_OK ? raw : -1;
}

void input_init(void)
{
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_BOOT) | (1ULL << PIN_ENC_SW) | (1ULL << PIN_JOY_SW) |
                        (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    pcnt_unit_config_t uc = { .high_limit = 30000, .low_limit = -30000 };
    ESP_ERROR_CHECK(pcnt_new_unit(&uc, &pcnt));
    pcnt_glitch_filter_config_t gf = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt, &gf));
    pcnt_channel_handle_t ca, cb;
    pcnt_chan_config_t cca = { .edge_gpio_num = PIN_ENC_A, .level_gpio_num = PIN_ENC_B };
    pcnt_chan_config_t ccb = { .edge_gpio_num = PIN_ENC_B, .level_gpio_num = PIN_ENC_A };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt, &cca, &ca));
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt, &ccb, &cb));
    pcnt_channel_set_edge_action(ca, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(ca, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(cb, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(cb, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt));

    adc_oneshot_unit_init_cfg_t ac = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ac, &adc));
    adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &cc));   // GPIO1
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_1, &cc));   // GPIO2

    // joystick present? both axes must rest near mid-scale and sit very
    // still: a pot wiper is low impedance and reads within a few counts,
    // a floating pin wanders by tens of counts
    int minx = 9999, maxx = 0, miny = 9999, maxy = 0; long sx = 0, sy = 0;
    for (int i = 0; i < 64; i++) {
        if (i % 8 == 7) vTaskDelay(1);
        int x = adc_read(ADC_CHANNEL_0), y = adc_read(ADC_CHANNEL_1);
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
        sx += x; sy += y;
    }
    jx0 = sx / 64.0f; jy0 = sy / 64.0f;
    has_joy = jx0 > 1200 && jx0 < 2900 && jy0 > 1200 && jy0 < 2900 && maxx - minx < 24 && maxy - miny < 24;
    ESP_LOGI(TAG, "joystick %s (centre %d,%d spread %d,%d)", has_joy ? "found" : "not found",
             (int)jx0, (int)jy0, maxx - minx, maxy - miny);
}

bool input_encoder_held_now(void) { return gpio_get_level(PIN_ENC_SW) == 0; }

static float axis(int raw, float centre)
{
    float v = raw > centre ? (raw - centre) / (4095.0f - centre) : (raw - centre) / centre;
    float a = fabsf(v);
    if (a < 0.12f) return 0;
    v = (a - 0.12f) / 0.88f * (v < 0 ? -1 : 1);
    return v < -1 ? -1 : (v > 1 ? 1 : v);
}

static void flick(float v, bool *neg, bool *pos, int idx)
{
    if (v > 0.7f && flick_armed[idx]) { *pos = true; flick_armed[idx] = false; }
    else if (v < -0.7f && flick_armed[idx]) { *neg = true; flick_armed[idx] = false; }
    else if (fabsf(v) < 0.3f) flick_armed[idx] = true;
}

void input_poll(input_ev_t *ev)
{
    memset(ev, 0, sizeof *ev);
    int cnt = 0;
    pcnt_unit_get_count(pcnt, &cnt);
    pcnt_unit_clear_count(pcnt);
    enc_acc += cnt;
    ev->enc = enc_acc / CONFIG_KIT_ENC_STEPS;
    enc_acc -= ev->enc * CONFIG_KIT_ENC_STEPS;

    bool pressed = gpio_get_level(PIN_BOOT) == 0 || gpio_get_level(PIN_ENC_SW) == 0;
    int64_t now = esp_timer_get_time();
    if (pressed && !was_pressed) { t_press = now; long_fired = false; }
    int held = pressed ? (int)((now - t_press) / 1000) : 0;
    if (pressed && !long_fired && held >= 3000) { ev->longp = true; long_fired = true; }
    if (!pressed && was_pressed && !long_fired) {
        int ms = (int)((now - t_press) / 1000);
        if (ms >= 40 && ms < 500) ev->tap = true;
        else if (ms >= 500) ev->hold = true;
    }
    ev->pressed = pressed; ev->held_ms = held;
    was_pressed = pressed;

    static bool joy_was;
    bool joy = gpio_get_level(PIN_JOY_SW) == 0;
    if (!joy && joy_was) ev->joy_click = true;
    joy_was = joy;

    ev->has_joy = has_joy;
    if (has_joy) {
        float x = axis(adc_read(ADC_CHANNEL_0), jx0), y = axis(adc_read(ADC_CHANNEL_1), jy0);
        // a stick nobody touches returns to centre; a floating pin does not
        static int off_centre;
        if (fabsf(x) > 0.5f || fabsf(y) > 0.5f) { if (++off_centre > 800) { has_joy = false; ESP_LOGW(TAG, "joystick pinned off centre for 8 s, ignoring it"); } }
        else off_centre = 0;
        jx_f += 0.4f * (x - jx_f); jy_f += 0.4f * (y - jy_f);
        ev->jx = jx_f; ev->jy = jy_f;
        flick(jx_f, &ev->flick_l, &ev->flick_r, 0);
        flick(jy_f, &ev->flick_d, &ev->flick_u, 1);
    }
}
