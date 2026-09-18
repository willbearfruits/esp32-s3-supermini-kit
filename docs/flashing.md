# Flashing without ESP-IDF

Every [release](https://github.com/willbearfruits/esp32-s3-supermini-kit/releases)
ships one image per application, each a single file that flashes at offset 0:

| File | Application |
|------|-------------|
| `kit-jam.bin` | JAM, the step sequencer and sampler (what the boxed kit runs) |
| `kit-looper.bin` | the voice looper |
| `kit-test.bin` | hardware test: mic to DAC with a wiring diagnosis on the screen |

## With esptool (Linux, macOS, Windows)

```sh
pip install esptool
esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x0 kit-jam.bin
```

Use the port your system gives the board: `/dev/ttyACM0` on Linux,
`/dev/cu.usbmodem*` on macOS, `COM3` or similar on Windows. Leave `--port`
out and esptool tries them all.

## In the browser

Open [esptool-js](https://espressif.github.io/esptool-js/) in Chrome or Edge,
connect, choose the file with flash address `0x0`, program. No install.

## If the board does not show up

The SuperMini uses the S3's native USB, so it normally enumerates by itself.
If it does not, or the flash fails partway: hold BOOT, tap RESET, release
BOOT, then flash again. The board reboots into the new firmware afterwards.
A cable that only carries power is the usual cause of a board that never
appears.

## After flashing

JAM saves its patterns on the board; a first boot writes defaults. The
hardware test image is the one to flash when something is wired wrong: it
names the likely missing wire on the screen and in the serial log at
115200 baud.

## Building the images yourself

`tools/release.sh` with ESP-IDF sourced builds all three and merges each with
its bootloader and partition table into `release/<version>/`.
