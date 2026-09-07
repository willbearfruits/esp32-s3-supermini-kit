#include "imu.h"
#include <math.h>
#include "esp_log.h"

static const char *TAG = "imu";
static i2c_master_dev_handle_t dev;
static bool present;
static float roll_f, pitch_f;

static bool wr(uint8_t reg, uint8_t val) { uint8_t b[2] = { reg, val }; return i2c_master_transmit(dev, b, 2, 50) == ESP_OK; }

bool imu_init(i2c_master_bus_handle_t bus)
{
    if (i2c_master_probe(bus, 0x68, 20) != ESP_OK) { ESP_LOGI(TAG, "no MPU6050"); return false; }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x68, .scl_speed_hz = 400000 };
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) return false;
    uint8_t who = 0, reg = 0x75;
    i2c_master_transmit_receive(dev, &reg, 1, &who, 1, 50);
    present = wr(0x6B, 0x01) && wr(0x1C, 0x00) && wr(0x1B, 0x00) && wr(0x1A, 0x03);   // wake, 2 g, 250 dps, 44 Hz DLPF
    ESP_LOGI(TAG, "MPU6050 who_am_i 0x%02X, %s", who, present ? "ready" : "init failed");
    return present;
}

bool imu_present(void) { return present; }

void imu_poll(float *roll_deg, float *pitch_deg)
{
    if (present) {
        uint8_t reg = 0x3B, b[6];
        if (i2c_master_transmit_receive(dev, &reg, 1, b, 6, 50) == ESP_OK) {
            float ax = (int16_t)(b[0] << 8 | b[1]), ay = (int16_t)(b[2] << 8 | b[3]), az = (int16_t)(b[4] << 8 | b[5]);
            float roll = atan2f(ay, az) * 57.2958f;
            float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2958f;
            roll_f += 0.25f * (roll - roll_f);
            pitch_f += 0.25f * (pitch - pitch_f);
        }
    }
    *roll_deg = roll_f; *pitch_deg = pitch_f;
}
