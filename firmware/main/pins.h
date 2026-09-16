// Pin map for the ESP32-S3 SuperMini kit.
// Keep this in sync with docs/pinmap.md.
#pragma once

// --- Joystick (analog, must stay on ADC1 = GPIO1..GPIO10, ADC2 dies with WiFi)
#define PIN_JOY_X     2
#define PIN_JOY_Y     1
#define PIN_JOY_SW    3   // active low, internal pull-up

// --- Rotary encoder
#define PIN_ENC_A     4
#define PIN_ENC_B     5
#define PIN_ENC_SW    6   // active low, internal pull-up

// --- I2S, one port in full duplex. Clocks shared by mic and both DACs.
#define PIN_I2S_BCLK  7   // -> INMP441 SCK, PCM5102A BCK, MAX98357A BCLK
#define PIN_I2S_WS    8   // -> INMP441 WS,  PCM5102A LCK, MAX98357A LRC
#define PIN_I2S_DOUT  9   // -> PCM5102A DIN, MAX98357A DIN. PCM5102A SCK must go to GND and its XSMT bridge to H, or it is silent.
#define PIN_I2S_DIN   10  // <- INMP441 SD

// --- Speaker amp control
#define PIN_AMP_SD    11  // MAX98357A SD_MODE: low = shutdown, high = (L+R)/2

// --- I2C bus: OLED (0x3C), ToF (0x29), IMU (0x68)
#define PIN_I2C_SDA   12
#define PIN_I2C_SCL   13

// --- Spare
#define PIN_SPARE     21  // ToF XSHUT or IMU INT later
