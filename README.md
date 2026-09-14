# ESP32-S3 SuperMini Audio Kit

An open-source, solderless, 3D-printable handheld built around the ESP32-S3
SuperMini. Microphone in, speaker and headphones out, a small OLED, an encoder,
a joystick, and room for a distance sensor and an IMU.

This repo will grow into everything needed to build one or sell one: firmware,
carrier PCB, enclosure, bill of materials, and assembly instructions.

**Status: milestone 4, the voice looper.** Hum, beatbox or whistle into a
handheld and get a track: up to 8 tracks on a fixed grid, scenes and an
arrangement, stereo mix, autosave, WAV/MIDI export over a USB drive mode.
The older eight-mode voice instrument is still selectable at build time.

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

The current [minimal purchasing list](parts/minimal-kit.md) is **your carrier PCB
plus seven parts**: ESP32-S3, purple PCM5102A board, joystick board with pins,
bare encoder, round six-pin I2S mic, 0.96-inch I2C OLED and MPU6050 board.
Available as [PDF](parts/minimal-kit.pdf) and [CSV](parts/minimal-kit.csv).
The [earlier expanded research](parts/purchasing-kit.md) keeps the additional
options and CAD references. Shipping and exact module dimensions remain unverified.

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

## Using the looper

One encoder (BOOT stands in for its push until one is wired), one joystick,
optional tilt sensor and distance sensor. Same grammar everywhere:

| Gesture | Does |
|---------|------|
| turn | next / previous track (in the menu: move, or change a value) |
| push | record a take on this track; push again to cancel |
| hold 0.6 s | menu (in the menu: back) |
| hold 3 s | clear all patterns |
| joystick tilt | expression: brightness and pitch bend |
| joystick click | mute this track |
| joystick flick up / down | next / previous scene, switched at the loop start |
| joystick flick left / right | track |
| body tilt | vibrato (roll) and brightness (pitch) |
| hand over the distance sensor | space (reverb send) |

A take is exactly one loop. Push, the metronome counts in one bar (or waits
for the loop to come round once something is playing), the take records for
one loop and stops by itself. Takes overdub by default; Take -> Replace next
or Undo change that. Drums are beatboxed (kick / snare / hat by the sound's
band energies), bass, keys and lead are hummed or whistled and snapped to
the grid and the key, the vocal track is a hard-tuned voice recorded as
audio. The key locks on the first melodic take, or set it in Song -> Key.

Menu sections: **Take** (undo, replace, copy A here, clear), **Track**
(sound, volume, pan, reverb, delay, low cut, tone, mute, solo, add / delete
track), **Scene** (scene, song mode, arrangement), **Song** (tempo, bars,
key, count-in, metronome, quantise, swing, humanise, kit, mic gain, gate),
**Master** (pump, drive, tone), **Files** (export, save / load slot, new,
USB drive).

USB drive mode (Files -> USB drive, or hold the encoder at power-up) shows
the storage partition to the PC and makes the board a MIDI device. Drop
`kick.wav`, `snare.wav`, `hat.wav` into `kits/user/` for your own drum kit;
exports land in `export/` as one WAV per track, a stereo `mix.wav`,
`song.mid` and `song.txt`. Hold the button to return. Flashing needs the
board out of USB mode (or BOOT held at power-up).

The serial log prints the current track, scene, key, the voice tracker's
level and confidence, the DSP load with a per-section breakdown, and which
inputs were detected.

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
