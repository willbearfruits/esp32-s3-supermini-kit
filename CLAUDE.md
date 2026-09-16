# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An open-source, solderless ESP32-S3 SuperMini audio handheld kit: INMP441 mic,
PCM5102A line-out DAC, MAX98357A speaker amp, SSD1306 OLED, encoder, joystick,
optional VL53L0X ToF and MPU6050 IMU. The repo will grow into firmware, a KiCad
carrier PCB, a printable enclosure, BOM and assembly guide. Firmware is MIT,
hardware will be CERN-OHL-P. Public at github.com/willbearfruits/esp32-s3-supermini-kit.

## Build and flash

ESP-IDF v5.5 lives at `~/esp/esp-idf` (not on PATH by default). Every firmware
command needs the environment sourced first:

```sh
. ~/esp/esp-idf/export.sh
cd firmware
idf.py set-target esp32s3      # once, or after deleting build/
idf.py build
idf.py flash monitor           # board enumerates over native USB (USB Serial/JTAG)
idf.py menuconfig              # tunables under "Kit firmware"
tools/app.sh looper|instrument|test|faust|jam   # switch the application in sdkconfig and rebuild
```

`sdkconfig` is generated and gitignored; edit `sdkconfig.defaults` for anything
that must persist. There is no test suite; verification is flashing a board and
reading the serial log described in README.md. Build logs on failure are under
`firmware/build/log/`.

## Firmware architecture

Single ESP-IDF app in `firmware/main/`, project name `kit_instrument`, three
applications selected by the Kconfig choice `KIT_APP` (menuconfig -> Kit
firmware -> Application, or `tools/app.sh`): the voice looper (default), the
older eight-mode voice instrument, a hardware test, a Faust showcase and JAM. All share the engine
below; `main.c` does the common bring-up (pin self-test, I2C bus recovery and
scan, OLED, `audio_start`) and then calls `app_<name>_run`. `fx_list.c` picks
the fx table per application. The build uses `-O3
-ffast-math`, PSRAM (2 MB embedded quad) and a custom `partitions.csv`
(1.5 MB app, 2.4 MB wear-levelled FAT at `/storage`).

- `pins.h` is the canonical pin map and must stay in sync with `docs/pinmap.md`.
  Joystick VRy on GPIO1, VRx on GPIO2 (ADC1, `input.c` derives the channels
  from the pins) + switch GPIO3, encoder GPIO4/5 + GPIO6, I2S 7-10,
  amp shutdown 11, I2C 12/13. BOOT (GPIO0) doubles as the encoder push until
  one is wired; a reset while it is held lands in download mode.
- `audio.c` owns I2S port 0 full duplex at 32 kHz, 64-frame blocks, 3 DMA
  descriptors (~6 ms round trip), 32-bit slots. The mic's data is in the
  left slot when its L/R pin is low and the right slot when high; the task
  detects which slot is alive (`audio_mic_slot`) and uses it. Low 8 bits are
  junk (masked). Input is float, high-passed at
  80 Hz, fed to `voice.c`, then to `fx_list[mode]->process()`. An fx with
  `stereo` set writes interleaved L/R. A cycle-counter CPU meter is exposed
  as `audio_cpu_load()`. Runs pinned to core 1.
- `voice.c`: level gate with start-up noise calibration and hysteresis, YIN
  pitch on a decimated 8 kHz copy every 8 ms, median of five, settle/drift
  note model, `since_onset` for latency compensation. Gate threshold and mic
  gain are runtime settable.
- `dsp.h`: RBJ biquads, TPT state variable filter (`svf_set` is cheap enough
  per sample), `fast_tan`/`fast_sin01`, note helpers. Keep libm out of
  per-sample loops and keep recirculating paths away from denormals (the S3
  FPU emulates them in software; `mix.c` adds a 1e-9 offset).
- Looper (`CONFIG_KIT_APP_LOOPER`): `looper.c` is the engine inside the
  audio task: up to 8 tracks of kinds drums/bass/keys/lead/vocal, 4 patterns
  per track chosen by scene (empty pattern falls back to A), arrangement of
  scenes, fixed grid with count-in, one take = one loop, overdub/replace/undo,
  swing/humanise, key auto-lock on the first melodic take or fixed. UI talks
  to it through `looper_action`/`looper_param_*`/`looper_get_ui`; actions are
  queued and run on the audio thread. `synth.c` (polyBLEP + SVF, presets),
  `kit.c` (3 synth kits + user WAV kit, beatbox classifier), `tune.c` (hard
  tune/harmony/robot/raw), `mix.c` (stereo mixer, sends, ping-pong delay,
  reverb in internal RAM, sidechain, oversampled clip, limiter), `scale.c`.
- `app_looper.c` is the UI task: `input.c` (encoder via PCNT, joystick via
  ADC with presence detection, BOOT alias), `imu.c` (MPU6050 tilt), `tof.c`
  (VL53L0X distance), two-level menu, autosave via `song.c` (state file per
  slot under `/storage/songs/N`, mu-law vocal files, WAV/MIDI export by
  real-time bounce), `usbmode.c` (TinyUSB mass storage + MIDI, entered from
  the menu or by holding the encoder at boot; console and flashing are gone
  while it runs, hold to leave).
- Instrument (`CONFIG_KIT_APP_INSTRUMENT`): `app_instrument.c` plus the
  `fx_*.c` modes; untested at 32 kHz.
- Test (`CONFIG_KIT_APP_TEST`): `app_test.c` with `fx_sine.c`, `input.c` and
  the instrument's delay/reverb/stutter. Mic through the chosen effect to the
  DACs; OLED and serial log show level, note, waveform and a one-line
  diagnosis built from the raw I2S words (no data, wrong slot, stuck line,
  loose wire), plus reset reason and an RTC boot counter for the first seconds
  so brownout loops are visible without serial. A 440 Hz tone plays for 4 s
  after boot as a DAC check. BOOT/encoder tap = next effect, hold 0.5 s =
  next preset (the optional `preset` hook in `fx_t`). This is the app to
  flash when bringing up a new board or debugging wiring.
- JAM (`CONFIG_KIT_APP_JAM`): `app_jam.c` UI + `jam.c` engine (fx_jam,
  stereo). Six channels (drums K/S/H, bass, lead, pad chords, sample, mic)
  on a 32-step grid; melodic rows are degrees of a chosen scale (7, 6 or 5
  rows + octave), pad rows are chord degrees. Bass and lead are the Faust
  voice in `faust/jam_voice.dsp` through `faust_voice.cpp`, pad is
  `synth.c`, drums `kit.c`, master `mix.c` (oversampling off). Sampler
  records from the mic channel into PSRAM and can be scratched with the
  joystick in perform mode. Three pages: PATTERN, MIX, SETUP; BOOT hold
  switches page, tap switches channel, encoder edits, joystick click toggles
  perform. Mic is only in the mix while its channel is selected. Calls
  `voice_set_pitch(false)`: no YIN, saves ~25% CPU. No save yet.
- Faust showcase (`CONFIG_KIT_APP_FAUST`): same UI as the test app, fx list
  from `fx_faust.cpp`. Programs live in `firmware/faust/*.dsp`; run
  `tools/faustgen.sh` (needs `faust` on PATH) after editing one. It writes
  `main/faust/<name>.h` (class `kfx_<name>`, committed so builds never need
  Faust). `fx_faust.cpp` collects sliders by label, applies named presets,
  writes `freq`/`gate` from the voice tracker when a program has them, and
  places objects over 40 KB in PSRAM. Faust base headers are vendored under
  `main/faust/`. Avoid table oscillators (`os.osc`, `os.oscsin`): each puts a
  256 KB static table in internal RAM; use `sin(2*ma.PI*os.lf_sawpos(f))`.
  Generate with `-ftz 1`, the mask variant does not compile here.
- `selftest.c` runs at boot: I2S pin short test and mic line pull test.
- `oled.c` is a minimal SSD1306 driver on `esp_lcd` with 5x7 and 2x text,
  circles, lines, bitmaps. Other I2C addresses: ToF 0x29, IMU 0x68.
- Adding a source file means listing it in `main/CMakeLists.txt` under the
  right application. `esp_tinyusb` comes from `main/idf_component.yml`.
- Reading the serial log from scripts: opening the port toggles DTR/RTS and
  can reset the chip; `esptool.py --after hard_reset chip_id` puts it back in
  the app.

## Docs, parts and mechanical folders

- `docs/pinmap.md` is the wiring reference, including the PCM5102A
  solder-bridge settings (FLT L, DEMP L, XSMT H, FMT L) that trip people up.
  `docs/pinmap.html` is generated and gitignored. `docs/acoustics.md` explains
  the Helmholtz maths the OpenSCAD files use.
- `enclosure/` is the current printable case: `enclosure.scad` (parametric,
  `part=` selects preview/exploded/assembled/shell/lid), `parts.scad`
  (component models and cutouts), `print_bed.scad`. `./render.sh` needs
  OpenSCAD on PATH and regenerates `out/*.png` and `out/*.stl`, which are
  kept in the repo. Designed for a 0.8 mm nozzle (walls in multiples of 0.8 mm) and
  heat-set inserts; keep those constraints when changing dimensions. Its
  README is the assembly guide.
- `3d/` holds the earlier concept bodies (slab, ocarina, cup) sharing their
  own `parts.scad` whose module sizes are typical values, not measurements.
- `parts/minimal-kit.md` is the current purchasing list (carrier PCB plus
  seven parts, listing leads only, nothing verified). `purchasing-kit.md` is
  the older expanded research; `build-purchasing-doc.py` regenerates its
  HTML/CSV and the resource manifest from the markdown (needs the Python
  `markdown` package). `research-2026-09-09/` has downloaded datasheets and
  drawings.
- `docs/pinout.md`, `images/` and `hardware/{schematic,pcb,manufacturing}`
  are scaffold placeholders for the KiCad carrier PCB milestone.
