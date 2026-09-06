// Milestone 1: hardware bring-up test for mic + DAC + OLED.
//
// What it does, in order:
//   1. Scans the I2C bus and logs every address that answers.
//   2. Brings up the SSD1306 OLED (if 0x3C answered) and shows a status page.
//   3. Plays a 440 Hz tone on the DACs for 2 s so the PCM5102A can be checked
//      without a working mic.
//   4. Loops the INMP441 mic to the DACs forever, showing a peak meter and a
//      live waveform on the OLED, and printing the peak once a second.
//
// All three audio chips share one I2S port in full duplex (see pins.h).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "pins.h"
#include "oled.h"

static const char *TAG = "hwtest";

#ifdef CONFIG_KIT_AMP_ALWAYS_ON
#define AMP_ALWAYS_ON 1
#else
#define AMP_ALWAYS_ON 0
#endif

#define OLED_ADDR 0x3C

#define FRAMES_PER_BLOCK 256                     // stereo frames per read/write
#define SAMPLES_PER_BLOCK (FRAMES_PER_BLOCK * 2) // L + R
#define BLOCK_BYTES (SAMPLES_PER_BLOCK * sizeof(int32_t))

#define TONE_HZ      440
#define TONE_SECONDS 2
#define TONE_AMPL    (0.25f * 2147483647.0f)     // -12 dBFS

// --- shared between audio task and display loop --------------------------

typedef enum { MODE_TONE, MODE_LOOPBACK } mode_t_;

typedef struct {
    volatile mode_t_ mode;
    volatile int32_t peak_l;      // raw peak since last display read
    volatile int32_t peak_r;
    volatile char    slot;        // 'L' or 'R': which slot carries the mic
    volatile int64_t last_alive;  // esp_timer time of last non-silent block
    int8_t           wave[OLED_W];// last block, 1 point per column
} stats_t;

static stats_t st = { .slot = 'L' };

// --- setup -----------------------------------------------------------------

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
    return bus;
}

// Probe every 7-bit address; returns count and fills `found`.
static int i2c_scan(i2c_master_bus_handle_t bus, uint8_t *found, int max)
{
    int n = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            ESP_LOGI(TAG, "I2C device at 0x%02X%s", a,
                     a == 0x3C ? " (SSD1306 OLED)" :
                     a == 0x29 ? " (VL53L0X ToF)" :
                     a == 0x68 ? " (MPU6050 IMU)" : "");
            if (n < max) found[n] = a;
            n++;
        }
    }
    if (n == 0) ESP_LOGW(TAG, "I2C scan: nothing answered on SDA=%d SCL=%d",
                         PIN_I2C_SDA, PIN_I2C_SCL);
    return n;
}

static void i2s_setup(i2s_chan_handle_t *tx, i2s_chan_handle_t *rx)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx, rx));

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
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx));
}

// Read back our own I2S output pins to prove the ESP is actually driving
// them. A pin that never changes level means the peripheral is not clocking
// (firmware problem); a pin that toggles but the modules stay dead means the
// wire, power, or module straps are the problem.
static void pin_selftest(const char *name, int pin)
{
    gpio_input_enable(pin); // leaves the I2S output routing untouched
    int highs = 0;
    const int n = 20000;
    for (int i = 0; i < n; i++) highs += gpio_get_level(pin);
    if (highs == 0 || highs == n) {
        ESP_LOGE(TAG, "self-test %s GPIO%d: STUCK %s - I2S is not driving it",
                 name, pin, highs ? "HIGH" : "LOW");
    } else {
        ESP_LOGI(TAG, "self-test %s GPIO%d: toggling (%d%% high) OK",
                 name, pin, highs * 100 / n);
    }
}

// Before I2S starts: drive each I2S pin in turn and see whether any other
// pin follows it. Two pins that follow each other both ways are shorted
// (adjacent breadboard rows, a wire in the wrong hole, a solder bridge).
static void short_test(void)
{
    const int pins[] = { PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_AMP_SD };
    const int n = sizeof pins / sizeof pins[0];
    int shorts = 0;
    for (int d = 0; d < n; d++) {
        for (int y = 0; y < n; y++) {
            if (y == d) continue;
            gpio_reset_pin(pins[y]);
            gpio_set_direction(pins[y], GPIO_MODE_INPUT);
            gpio_reset_pin(pins[d]);
            gpio_set_direction(pins[d], GPIO_MODE_OUTPUT);

            gpio_set_pull_mode(pins[y], GPIO_PULLDOWN_ONLY);
            gpio_set_level(pins[d], 1);
            vTaskDelay(1);
            int follows_high = gpio_get_level(pins[y]) == 1;

            gpio_set_pull_mode(pins[y], GPIO_PULLUP_ONLY);
            gpio_set_level(pins[d], 0);
            vTaskDelay(1);
            int follows_low = gpio_get_level(pins[y]) == 0;

            if (follows_high && follows_low && d < y) {
                ESP_LOGE(TAG, "short-test: GPIO%d and GPIO%d are SHORTED together",
                         pins[d], pins[y]);
                shorts++;
            }
        }
    }
    for (int i = 0; i < n; i++) gpio_reset_pin(pins[i]);
    if (!shorts) ESP_LOGI(TAG, "short-test: no shorts between I2S pins");

    // Mic data line: does anything hold it, or does it just follow our pulls?
    gpio_set_direction(PIN_I2S_DIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLDOWN_ONLY);
    vTaskDelay(1);
    int with_down = gpio_get_level(PIN_I2S_DIN);
    gpio_set_pull_mode(PIN_I2S_DIN, GPIO_PULLUP_ONLY);
    vTaskDelay(1);
    int with_up = gpio_get_level(PIN_I2S_DIN);
    gpio_reset_pin(PIN_I2S_DIN);
    if (with_down == 0 && with_up == 1) {
        ESP_LOGW(TAG, "pull-test GPIO%d: floating, nothing connected or mic is high-Z", PIN_I2S_DIN);
    } else {
        ESP_LOGW(TAG, "pull-test GPIO%d: held %s by something external (mic SD, or a wire to %s?)",
                 PIN_I2S_DIN, with_up ? "HIGH" : "LOW", with_up ? "3V3" : "GND");
    }
}

// While I2S is clocking, look at every spare GPIO for a signal that toggles.
// If the mic's SD wire landed on the wrong header pin, this finds it.
static void stray_signal_scan(void)
{
    const int cand[] = { 1, 2, 3, 4, 5, 14, 15, 16, 17, 18, 21 };
    int found = 0;
    for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        int pin = cand[i];
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_FLOATING);
        int prev = gpio_get_level(pin), edges = 0;
        for (int k = 0; k < 5000; k++) {
            int v = gpio_get_level(pin);
            if (v != prev) edges++;
            prev = v;
        }
        if (edges > 20) {
            ESP_LOGW(TAG, "stray signal on GPIO%d (%d edges): is the mic SD wire on this pin instead of GPIO%d?",
                     pin, edges, PIN_I2S_DIN);
            found++;
        }
        gpio_reset_pin(pin);
    }
    if (!found) ESP_LOGI(TAG, "stray-scan: no data-like signal on any spare GPIO");
}

// --- audio task --------------------------------------------------------------

static float to_dbfs(int32_t peak)
{
    return peak > 0 ? 20.0f * log10f((float)peak / 2147483648.0f) : -120.0f;
}

static inline int32_t clamp32(int64_t v)
{
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return (int32_t)v;
}

static void audio_task(void *arg)
{
    i2s_chan_handle_t tx = NULL, rx = NULL;
    i2s_setup(&tx, &rx);

    int32_t *buf = malloc(BLOCK_BYTES);
    assert(buf);

    // Phase 1: tone on the DACs. The RX side keeps running so the mic clocks
    // are stable, we just ignore what it returns.
    st.mode = MODE_TONE;
    ESP_LOGI(TAG, "DAC test: %d Hz tone for %d s", TONE_HZ, TONE_SECONDS);
    float phase = 0.0f;
    const float dphase = 2.0f * (float)M_PI * TONE_HZ / CONFIG_KIT_SAMPLE_RATE;
    int blocks = (TONE_SECONDS * CONFIG_KIT_SAMPLE_RATE) / FRAMES_PER_BLOCK;
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < SAMPLES_PER_BLOCK; i += 2) {
            int32_t s = (int32_t)(sinf(phase) * TONE_AMPL);
            buf[i] = s;
            buf[i + 1] = s;
            phase += dphase;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
            // Fake a waveform so the screen shows something during the tone.
            if ((i >> 1) % (FRAMES_PER_BLOCK / OLED_W) == 0) {
                st.wave[(i >> 1) / (FRAMES_PER_BLOCK / OLED_W)] = s >> 24;
            }
        }
        size_t put = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx, buf, BLOCK_BYTES, &put, portMAX_DELAY));
        if (b == 4) {
            pin_selftest("BCLK", PIN_I2S_BCLK);
            pin_selftest("WS  ", PIN_I2S_WS);
            pin_selftest("DOUT", PIN_I2S_DOUT);
            stray_signal_scan();
            gpio_setup();
        }
        // Drain RX so its DMA never overflows.
        size_t got = 0;
        i2s_channel_read(rx, buf, BLOCK_BYTES, &got, 0);
        if (b == blocks / 2 && got >= 8 * sizeof(int32_t)) {
            int32_t pl = 0, pr = 0;
            for (size_t f = 0; f < got / 8; f++) {
                int32_t l = abs(buf[2 * f]), r = abs(buf[2 * f + 1]);
                if (l > pl) pl = l;
                if (r > pr) pr = r;
            }
            ESP_LOGI(TAG, "during tone, RX sees L %6.1f dBFS R %6.1f dBFS "
                     "(-12 here = DIN shorted/coupled to DOUT), %u bytes, raw %08lX %08lX %08lX %08lX",
                     to_dbfs(pl), to_dbfs(pr), (unsigned)got,
                     (unsigned long)buf[0], (unsigned long)buf[1], (unsigned long)buf[2], (unsigned long)buf[3]);
        }
    }

    // Phase 2: mic -> DACs forever.
    st.mode = MODE_LOOPBACK;
    ESP_LOGI(TAG, "mic loopback: gain x%d, amp %s", CONFIG_KIT_MIC_GAIN,
             AMP_ALWAYS_ON ? "always on" : "on while encoder button held");

    int32_t log_peak = 0;
    int64_t last_log = esp_timer_get_time();

    while (1) {
        size_t got = 0;
        ESP_ERROR_CHECK(i2s_channel_read(rx, buf, BLOCK_BYTES, &got, portMAX_DELAY));
        size_t frames = got / (2 * sizeof(int32_t));

        // Find which slot the mic is in. L/R pin to GND -> left slot.
        uint32_t raw[8];
        memcpy(raw, buf, sizeof raw);
        // Decide per slot whether real INMP441 data is present. The mic is
        // 24-bit left-justified and releases the line after bit 24, so the
        // low 8 bits of each word are junk (typically 0x7F). Mask them off,
        // then a valid word is anything that is not all-0 or all-1. A
        // floating line only ever gives those two.
        int32_t pl = 0, pr = 0;
        size_t valid_l = 0, valid_r = 0;
        for (size_t f = 0; f < frames; f++) {
            int32_t l = buf[2 * f] & (int32_t)0xFFFFFF00;
            int32_t r = buf[2 * f + 1] & (int32_t)0xFFFFFF00;
            buf[2 * f] = l;
            buf[2 * f + 1] = r;
            if (l != 0 && l != (int32_t)0xFFFFFF00) valid_l++;
            if (r != 0 && r != (int32_t)0xFFFFFF00) valid_r++;
            l = l < 0 ? -l : l;
            r = r < 0 ? -r : r;
            if (l > pl) pl = l;
            if (r > pr) pr = r;
        }
        bool alive_l = valid_l > frames / 2, alive_r = valid_r > frames / 2;
        bool dead = !alive_l && !alive_r;
        int use_r = alive_r && !alive_l;
        if (dead) {
            // No mic: keep the test tone running so the DAC can still be
            // probed, instead of feeding line noise to it.
            pl = pr = 0;
            for (size_t f = 0; f < frames; f++) {
                int32_t t = (int32_t)(sinf(phase) * TONE_AMPL);
                buf[2 * f] = t;
                buf[2 * f + 1] = t;
                phase += dphase;
                if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
            }
        }
        st.slot = use_r ? 'R' : 'L';
        if (pl > st.peak_l) st.peak_l = pl;
        if (pr > st.peak_r) st.peak_r = pr;
        int32_t praw = use_r ? pr : pl;
        if (!dead) st.last_alive = esp_timer_get_time();
        if (praw > log_peak) log_peak = praw;

        // Gain, duplicate to both channels, capture waveform.
        const int step = frames / OLED_W > 0 ? frames / OLED_W : 1;
        for (size_t f = 0; f < frames; f++) {
            int32_t s = dead ? buf[2 * f]
                             : clamp32((int64_t)buf[2 * f + use_r] * CONFIG_KIT_MIC_GAIN);
            buf[2 * f] = s;
            buf[2 * f + 1] = s;
            if (f % step == 0 && f / step < OLED_W) st.wave[f / step] = s >> 24;
        }

#if AMP_ALWAYS_ON
        gpio_set_level(PIN_AMP_SD, 1);
#else
        gpio_set_level(PIN_AMP_SD, gpio_get_level(PIN_ENC_SW) == 0);
#endif

        size_t put = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx, buf, got, &put, portMAX_DELAY));

        int64_t now = esp_timer_get_time();
        if (now - last_log > 1000000) {
            float dbfs = log_peak > 0 ? 20.0f * log10f((float)log_peak / 2147483648.0f) : -120.0f;
            int bars = (int)((dbfs + 60.0f) / 2.0f);
            if (bars < 0) bars = 0;
            if (bars > 30) bars = 30;
            char meter[32];
            memset(meter, '#', bars);
            meter[bars] = '\0';
            ESP_LOGI(TAG, "raw L/R: %08lX %08lX  %08lX %08lX  %08lX %08lX  %08lX %08lX",
                     (unsigned long)raw[0], (unsigned long)raw[1], (unsigned long)raw[2], (unsigned long)raw[3],
                     (unsigned long)raw[4], (unsigned long)raw[5], (unsigned long)raw[6], (unsigned long)raw[7]);
            if (esp_timer_get_time() - st.last_alive > 900000) {
                ESP_LOGW(TAG, "mic: NO DATA, GPIO%d is floating - check INMP441 SD->GPIO%d, "
                         "SCK->GPIO%d, WS->GPIO%d, VDD, GND, L/R->GND",
                         PIN_I2S_DIN, PIN_I2S_DIN, PIN_I2S_BCLK, PIN_I2S_WS);
            } else {
                ESP_LOGI(TAG, "mic %c %6.1f dBFS |%-30s|", (char)st.slot, dbfs, meter);
            }
            log_peak = 0;
            last_log = now;
        }
    }
}

// --- display -----------------------------------------------------------------

static void draw_page(int n_i2c, const uint8_t *addrs)
{
    char line[32];
    oled_clear();

    snprintf(line, sizeof line, "KIT HW TEST %dk", CONFIG_KIT_SAMPLE_RATE / 1000);
    oled_text(0, 0, line);
    oled_hline(0, OLED_W - 1, 8, true);

    // I2C line
    int pos = snprintf(line, sizeof line, "I2C:");
    if (n_i2c == 0) pos += snprintf(line + pos, sizeof line - pos, " none");
    for (int i = 0; i < n_i2c && i < 4 && pos < (int)sizeof line - 3; i++)
        pos += snprintf(line + pos, sizeof line - pos, " %02X", addrs[i]);
    oled_text(0, 11, line);

    // Audio status line + bar
    int32_t peak = st.slot == 'R' ? st.peak_r : st.peak_l;
    st.peak_l = st.peak_r = 0;
    int64_t now = esp_timer_get_time();
    float dbfs = to_dbfs(peak);

    if (st.mode == MODE_TONE) {
        snprintf(line, sizeof line, "DAC: %dHz TONE", TONE_HZ);
    } else if (now - st.last_alive > 1500000) {
        snprintf(line, sizeof line, "NO MIC - DAC TONE");
    } else {
        snprintf(line, sizeof line, "MIC %c %6.1fdBFS", (char)st.slot, dbfs);
    }
    oled_text(0, 21, line);

    int w = (int)((dbfs + 60.0f) / 60.0f * (OLED_W - 2));
    if (w < 0) w = 0;
    if (w > OLED_W - 2) w = OLED_W - 2;
    oled_rect(0, 29, OLED_W, 5, false);
    if (st.mode == MODE_LOOPBACK) oled_rect(1, 30, w, 3, true);

    // Waveform in the bottom half, centre line dotted
    const int cy = 48;
    for (int x = 0; x < OLED_W; x += 4) oled_pixel(x, cy, true);
    int prev = cy - (st.wave[0] * 15) / 128;
    for (int x = 1; x < OLED_W; x++) {
        int y = cy - (st.wave[x] * 15) / 128;
        oled_vline(x, prev, y, true);
        prev = y;
    }

    oled_flush();
}

void app_main(void)
{
    ESP_LOGI(TAG, "kit hardware test: mic + DAC + OLED");
    gpio_setup();

    short_test();
    gpio_setup(); // short_test reset the amp pin, put it back
    i2c_master_bus_handle_t bus = i2c_setup();
    uint8_t addrs[8];
    int n_i2c = i2c_scan(bus, addrs, 8);

    bool have_oled = false;
    for (int i = 0; i < n_i2c && i < 8; i++) if (addrs[i] == OLED_ADDR) have_oled = true;
    if (have_oled) {
        if (oled_init(bus, OLED_ADDR) == ESP_OK) {
            ESP_LOGI(TAG, "OLED up");
            oled_text(0, 0, "KIT HW TEST");
            oled_text(0, 12, "OLED OK");
            oled_flush();
        }
    } else {
        ESP_LOGW(TAG, "no OLED at 0x%02X, continuing without display", OLED_ADDR);
    }

    st.last_alive = esp_timer_get_time();
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 5, NULL, 1);

    while (1) {
        if (oled_present()) draw_page(n_i2c, addrs);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
