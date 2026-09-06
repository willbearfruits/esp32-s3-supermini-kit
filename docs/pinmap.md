# Pin map

Board: **ESP32-S3 SuperMini** (ESP32-S3FH4R2, 4 MB flash, 2 MB PSRAM, native USB).
All logic is 3.3 V. Every module below runs from the SuperMini's 3V3 pin.

The canonical copy of this table lives in `firmware/main/pins.h`. If you change
one, change the other.

| GPIO | Name        | Goes to                                             | Notes |
|-----:|-------------|-----------------------------------------------------|-------|
| 1    | JOY_X       | Joystick VRx                                        | ADC1 |
| 2    | JOY_Y       | Joystick VRy                                        | ADC1 |
| 3    | JOY_SW      | Joystick SW                                         | active low, internal pull-up |
| 4    | ENC_A       | Encoder CLK / A                                     | |
| 5    | ENC_B       | Encoder DT / B                                      | |
| 6    | ENC_SW      | Encoder SW                                          | active low, internal pull-up |
| 7    | I2S_BCLK    | INMP441 **SCK**, PCM5102A **BCK**, MAX98357A **BCLK** | shared clock |
| 8    | I2S_WS      | INMP441 **WS**, PCM5102A **LCK**, MAX98357A **LRC**   | shared clock |
| 9    | I2S_DOUT    | PCM5102A **DIN**, MAX98357A **DIN**                 | ESP32 -> DACs |
| 10   | I2S_DIN     | INMP441 **SD**                                      | mic -> ESP32 |
| 11   | AMP_SD      | MAX98357A **SD**                                    | low = amp off |
| 12   | I2C_SDA     | OLED, ToF, IMU SDA                                  | |
| 13   | I2C_SCL     | OLED, ToF, IMU SCL                                  | |
| 21   | SPARE       | free                                                | ToF XSHUT or IMU INT later |

Pins deliberately left alone: 0 (BOOT button), 19/20 (USB), 43/44 (UART0),
48 (onboard RGB LED), 45/46 (strapping).

Why the joystick sits on GPIO1 and GPIO2: those are ADC1 channels. ADC2 stops
working while WiFi is active on the ESP32-S3, so never put analog inputs there.

## Why one I2S port is enough

The ESP32-S3 has two I2S ports. We use only port 0, in full duplex:

- The two DACs are receivers. Receivers can share a bus freely, so both get the
  same BCLK, WS and data line and play the same stream.
- The mic is a transmitter, so it needs its own data line into the ESP32, but
  it can still share BCLK and WS with the DACs.
- The only rule is that everyone runs at the same sample rate and slot width.
  We use 32-bit slots, stereo, Philips format. All three chips accept that.
- None of the three chips needs MCLK.

Total cost: 4 GPIOs for the entire audio path.

Trade-off: both DACs always play the same audio. If you ever want different
audio on speaker vs line-out, move one DAC to I2S port 1 (3 more pins).

## Module wiring

### INMP441 microphone (round board, 6 pins)

| INMP441 | Connect to |
|---------|------------|
| VDD     | 3V3 |
| GND     | GND |
| SD      | GPIO 10 |
| SCK     | GPIO 7 |
| WS      | GPIO 8 |
| L/R     | GND (data lands in the left slot) |

### PCM5102A DAC (purple board, line / headphone out)

| PCM5102A | Connect to |
|----------|------------|
| VIN      | 3V3 |
| GND      | GND |
| LCK      | GPIO 8 |
| DIN      | GPIO 9 |
| BCK      | GPIO 7 |
| SCK      | **GND. Required.** Leave it floating and the DAC never locks its clock and stays silent. The purple board does not ground it for you. |

**Two things must be right or the PCM5102A produces no sound at all:**

1. **SCK wired to GND** (see above).
2. **Solder bridge 3 (XSMT) bridged to H.** Boards ship with it open, and an
   open XSMT keeps the DAC muted forever.

Check the four solder bridges on the back. They must be:

| Bridge | Pin  | Set to |
|--------|------|--------|
| H1L    | FLT  | L |
| H2L    | DEMP | L |
| H3L    | XSMT | H |
| H4L    | FMT  | L |

Each bridge is three pads: H, centre, L. The centre pad is the chip's config
pin. Bridge it to H to tie it high, or to L to tie it low. The H pads are fed
from the board's onboard 3.3 V regulator, not straight from VIN, so a
continuity test from H to VIN reads open. That is normal.

Boards often ship with one or more of these open. This is the one component
in the kit that may need a soldering iron, so for a solderless kit either buy
pre-bridged boards or move these straps onto the carrier PCB.

Power note: VIN feeds a 3.3 V regulator on the board. Fed with 3.3 V it drops
to roughly 3.0 V, which works. Feeding VIN from 5 V is also safe, since only
the regulator sees it and the I2S inputs are still driven at 3.3 V.

### MAX98357A amplifier (speaker out)

| MAX98357A | Connect to |
|-----------|------------|
| VIN       | 3V3 (5V works too and is louder, but keep logic at 3.3 V) |
| GND       | GND |
| SD        | GPIO 11 |
| GAIN      | leave floating (9 dB) |
| DIN       | GPIO 9 |
| BCLK      | GPIO 7 |
| LRC       | GPIO 8 |
| + / -     | speaker, 4 or 8 ohm |

Driving SD high (3.3 V) selects the (L+R)/2 mix mode. Driving it low shuts the
amp down. The firmware starts with it low.

### I2C devices

All on GPIO 12 (SDA) and GPIO 13 (SCL). Addresses do not clash:

| Device            | Address |
|-------------------|---------|
| SSD1306 0.96" OLED | 0x3C |
| VL53L0X / VL53L1X ToF | 0x29 |
| MPU6050 IMU       | 0x68 |

Most breakout boards include pull-ups. If none of yours do, add 4.7 k to 3V3
on SDA and SCL.
