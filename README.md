# ESP32-S3 SuperMini Audio Kit

An open-source, solderless, 3D-printable handheld built around the ESP32-S3
SuperMini. Microphone in, speaker and headphones out, a small OLED, an encoder,
a joystick, and room for a distance sensor and an IMU.

This repo will grow into everything needed to build one or sell one: firmware,
carrier PCB, enclosure, bill of materials, and assembly instructions.

**Status: milestone 0.** The pin map is decided and the first firmware proves
the shared I2S audio bus by looping the mic straight to the outputs.

## Parts

| Part | Role | Bus |
|------|------|-----|
| ESP32-S3 SuperMini | brain, native USB | |
| INMP441 | MEMS microphone | I2S (RX) |
| PCM5102A | line / headphone DAC | I2S (TX) |
| MAX98357A | 3 W speaker amp | I2S (TX) |
| 0.96" SSD1306 OLED | display | I2C |
| Rotary encoder with button | input | GPIO |
| Analog joystick with button | input | ADC1 + GPIO |
| VL53L0X (optional) | time-of-flight distance | I2C |
| MPU6050 (optional) | gyro + accelerometer | I2C |

All three audio chips share one I2S port. See [docs/pinmap.md](docs/pinmap.md)
for the wiring and why that works.

## Repo layout

```
firmware/   ESP-IDF project (C)
docs/       pin map, wiring, later: BOM and assembly guide
hardware/   later: KiCad carrier PCB and printable enclosure
```

## Build the firmware

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html)
v5.5 or newer with the `esp32s3` target installed.

```sh
. ~/esp/esp-idf/export.sh        # or wherever your IDF lives
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

The SuperMini's USB-C is wired to the S3's native USB, so it usually flashes
without pressing anything. If it does not show up, hold BOOT, tap RESET,
release BOOT, then flash again.

Tunables live under `idf.py menuconfig` -> `Kit loopback`: mic gain, sample
rate, and whether the speaker amp is always on.

## What milestone 0 does

1. Opens I2S port 0 in full duplex at 16 kHz, 32-bit stereo.
2. Reads the mic, multiplies by the gain, copies left to right.
3. Writes the result back out to both DACs.
4. Prints a peak meter over USB once a second.
5. Keeps the speaker amp muted unless the encoder button is held. Mic-to-speaker
   loopback howls if the speaker is near the mic. Use headphones on the
   PCM5102A to hear it cleanly, or set `KIT_AMP_ALWAYS_ON` in menuconfig.

Expected serial output:

```
I (312) loopback: kit loopback: 16000 Hz, gain x8, amp on while encoder button held
I (1312) loopback: peak  -42.3 dBFS |#########                     |
I (2312) loopback: peak  -18.7 dBFS |####################          |
```

If the meter never moves, check the mic's SD wire and that L/R is tied to GND.
If the meter moves but you hear nothing on the PCM5102A, check its XSMT bridge.

## Roadmap

- [x] Pin map
- [x] I2S loopback (mic -> DACs)
- [ ] OLED + encoder + joystick input demo
- [ ] ToF and IMU readouts
- [ ] Carrier PCB (KiCad) with sockets for every module
- [ ] 3D-printed enclosure
- [ ] BOM with links and assembly guide

## License

Firmware and documentation: [MIT](LICENSE). Hardware files, when they land,
will be CERN-OHL-P.
