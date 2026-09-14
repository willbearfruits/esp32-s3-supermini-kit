# ESP32-S3 audio kit — purchasing shortlist

**Earlier expanded research.** The current build uses [seven purchased parts plus your carrier PCB](minimal-kit.md), with a joystick board with pins and a bare encoder. Use that minimal list for the current kit; the broader options below are retained as research references.

Research date: **9 September 2026**. Working quantity: **10 complete kits**. Encoder and joystick are **bare through-hole components, without breakout boards**; the other electronics are modules. Destination and this quantity assumption await confirmation.

**Purchase status: shipping and exact-product drawing checks are pending. No listing is yet confirmed to meet all your requirements.** This document contains real listing leads, manufacturer documentation and the remaining parts needed to finish the build. It is not an AliExpress cart, and no orders have been placed.

AliExpress product pages could not be read successfully through the research tool. The listing links below were identified through indexed Pricearchive/Alitools records; their titles establish candidate parts and advertised pack sizes only. Stock, selected variants, seller identity, authenticity, Choice eligibility, prices and delivery charges remain unverified. The adjacent “listing evidence” links preserve that distinction.

## Your buying requirements

- Prefer one **10-piece lot** per part; larger lots are acceptable. A listing headed “1–10 pieces” needs the **10-piece variant**, with cart quantity **1**. Ten single pieces or five pairs are exceptions, not automatically equivalent to the requested lot.
- Require **zero shipping charge to your delivery address** for the selected lot. I interpret “AliExpress special” as **Choice**, pending confirmation. Alibaba describes Choice shipping as conditional on market and purchase thresholds; its general description does not establish today's terms for your address. Check both the product and final basket. [Alibaba's Choice explanation](https://www.alibabagroup.com/en-US/document-1696296090350911488).
- Require a manual/datasheet with **dimensions and mounting information for the actual item supplied**. For modules this means the PCB, header positions, holes, connector heights and protrusions. A chip's package drawing alone does not qualify a module.
- Prefer DipTrace symbols/footprints and matching STEP models. A similar-looking component or another manufacturer's breakout is only a reference.

## Main components: one of each per kit

Every row below has a target of **10 physical pieces**. Optional sensors are included in that target so all ten kits can have motion and distance control. **Shipping/Choice: unverified for every row. Live lot price: unverified for every row.**

| ID | Part and exact selection | Advertised lot and AliExpress link | Documentation / remaining gap |
|---|---|---|---|
| K01 | **ESP32-S3 SuperMini, ESP32-S3FH4R2**, 4 MB flash, 2 MB PSRAM, native USB-C | **1–10 pcs**, select 10: [listing](https://www.aliexpress.com/item/1005012191736844.html) · [listing evidence](https://sv.pricearchive.org/aliexpress.com/item/1005012191736844) | Exact SuperMini PCB drawing not verified. Firmware requires this S3/PSRAM configuration; a C3 SuperMini is unsuitable. Documented Waveshare alternative below. |
| K02 | **INMP441 I2S microphone breakout**, with L/R pin accessible | **1/5/10 pcs**, select 10: [listing](https://www.aliexpress.com/item/1005009006830769.html) · [listing evidence](https://vi.pricearchive.org/aliexpress.com/item/1005009006830769) | [TDK chip datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf). Exact board dimensions, acoustic-port position and pin layout still needed. Do not select a different microphone from a mixed listing without checking it. |
| K03 | **PCM5102A stereo I2S DAC module**, preferably with fitted 3.5 mm line-output jack | **1–10 pcs**, select 10: [listing](https://www.aliexpress.com/item/1005010369195534.html) · [listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005010369195534) | [TI datasheet](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf) · [saved PDF](research-2026-09-09/PCM5102A-chip-datasheet.pdf). Exact PCB/jack drawing, regulator and mute straps still needed. This is line output; see headphone amplifier below. |
| K04 | **MAX98357A mono I2S speaker amplifier module**; require the **A** version | **10 pcs / 1 pc**, select 10: [listing](https://www.aliexpress.com/item/1005009759175798.html) · [listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005009759175798) | [Analog Devices datasheet/CAD](https://www.analog.com/en/products/MAX98357A.html). Listing title omits the suffix: confirm A, board dimensions, SD/GAIN circuitry and terminal block. A uses I2S; B uses left-justified audio. |
| K05 | **0.96-inch SSD1306 OLED**, 128×64, **4-pin I2C**, white | **10 pcs / lot**: [listing](https://www.aliexpress.com/item/1005008169257099.html) · [listing evidence](https://ro.pricearchive.org/aliexpress.com/item/1005008169257099) | Exact PCB, glass, active-area and mounting-hole drawing needed. Confirm 0x3C address, 3.3 V operation and header order. Do not substitute SH1106, 128×32 or SPI. |
| K06 | **Bare EC11 rotary encoder with push switch**, 3 encoder pins + 2 switch pins, vertical shaft | **10 pcs**, choose shaft style/length: [listing](https://www.aliexpress.com/item/1005005239756119.html) · [listing evidence](https://sv.pricearchive.org/aliexpress.com/item/1005005239756119) | **Hold for seller drawing.** Generic EC11 does not identify one footprint. Need body, lug spacing, shaft length/diameter, bushing, detents and pulses. Exact documented ALPS alternative below. No KY-040 board. |
| K07 | **Bare ALPS RKJXV1224005 joystick**, two 10 kΩ linear pots, spring return, center push | **10–100 pcs**, select 10: [listing](https://www.aliexpress.com/item/1005011995304794.html) · [listing evidence](https://ro.pricearchive.org/aliexpress.com/item/1005011995304794) | [ALPS exact-part documentation](https://tech.alpsalpine.com/e/products/detail/RKJXV1224005/). **17.8 × 21.3 × 11.2 mm**; dimension, mounting and lever drawings saved below. Best exact-model 10-piece lead found. Seller identity/authenticity and supplied model still need confirmation. Cap inclusion unknown. No KY-023 board. |
| K08 | **MPU6050 / GY-521 breakout**, gyro + accelerometer | **10 pcs**: [listing](https://www.aliexpress.com/item/1005011896641210.html) · [listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005011896641210) | [TDK chip datasheet](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf). Exact GY-521 PCB drawing needed. Require 3.3 V-compatible power/I2C and AD0 low for 0x68. Optional in firmware, included here. |
| K09 | **VL53L0X I2C distance-sensor breakout** | **10 pcs / 1 pc**, select specifically VL53L0X and 10: [listing](https://www.aliexpress.com/item/1005010777247203.html) · [listing evidence](https://uk.pricearchive.org/aliexpress.com/item/1005010777247203) | [ST datasheet](https://www.st.com/resource/en/datasheet/vl53l0x.pdf). Mixed-family listing: exact sensor, direct I2C interface, board drawing and voltage compatibility need confirmation. VL53L1X and serial-output TOF boards are not drop-in replacements for this firmware. Optional, included here. |
| K10 | **40 mm round speaker, 4 Ω, 3 W nominal**, with leads if possible | **1/2/10 pcs**, select 10 and 40 mm: [listing](https://www.aliexpress.com/item/1005009533204416.html) · [listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005009533204416) | **Hold for full drawing.** Diameter alone is insufficient: magnet depth, rim, terminals and mounting are needed. The enclosure's 5.5 mm speaker depth is an unmeasured placeholder. |
| K11 | **Stereo headphone amplifier**, if headphones are required; TDA1308 board is a candidate | **10 pcs / 1 pc**, select 10: [listing](https://www.aliexpress.com/item/1005009791615188.html) · [listing evidence](https://ms.pricearchive.org/aliexpress.com/item/1005009791615188) | **Design/sourcing hold.** [NXP TDA1308 documentation](https://www.nxp.com/products/TDA1308). Require schematic, dimensions, stereo headphone load rating, gain and output coupling. Listing calls it a microphone preamp, so its suitability is unconfirmed. TDA1308 is discontinued; do not commit a new production design on this lead alone. |

No total cost is provided: indexed starting prices often correspond to one piece or a different variant. The CSV leaves actual selected-lot price and shipping fields blank for verified quotes.

## The bare controls and their drawings

**Joystick K07:** ALPS documents the exact RKJXV1224005, including its center switch. It is marked **not recommended for new designs**. For a longer-lived design, consider the current **RKJXV122400R**, documented as **18.2 × 21.7 × 11.2 mm**. The two outlines differ; do not use one drawing for the other. A matching 10-piece AliExpress lot for the R version was not confirmed. [4005 specifications](https://tech.alpsalpine.com/e/products/detail/RKJXV1224005/) · [400R specifications](https://tech.alpsalpine.com/e/products/detail/RKJXV122400R/).

| Saved manufacturer drawing | What it establishes |
|---|---|
| [RKJXV1224005 dimensions](research-2026-09-09/ALPS-RKJXV1224005-dimensions.gif) | Body, terminals and overall geometry |
| [RKJXV1224005 mounting holes](research-2026-09-09/ALPS-RKJXV1224005-mounting.gif) | PCB hole layout; observe the manufacturer's mounting-side view |
| [RKJXV1224005 lever](research-2026-09-09/ALPS-RKJXV1224005-lever.gif) | Cap-interface geometry |
| [EC11E15244G1 dimensions](research-2026-09-09/ALPS-EC11E15244G1-dimensions.gif) | Exact ALPS encoder outline |
| [EC11E15244G1 mounting holes](research-2026-09-09/ALPS-EC11E15244G1-mounting.gif) | Exact ALPS encoder PCB hole layout |

**Encoder alternative:** [ALPS EC11E15244G1](https://tech.alpsalpine.com/e/products/detail/EC11E15244G1/) has a 20 mm flat shaft, 30 detents, 15 pulses and a push switch. An [AliExpress listing naming this exact model](https://www.aliexpress.com/item/1005008895804725.html) advertises **2 pieces** ([listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005008895804725)). Five packs would yield ten encoders, but **does not meet your 10-piece-lot requirement**. It is an alternative only if you accept that exception; shipping is also unverified. The saved ALPS drawings do **not** qualify the generic K06 lot.

Bare controls need soldering to a carrier PCB, including their mechanical support tabs. The assembled carrier can then form a plug-in kit. They will not plug straight into the five-pin breakout sockets shown in generic Arduino wiring diagrams.

## DipTrace libraries and 3D models

**No exact native DipTrace symbol/footprint set was verified for the AliExpress lots.** These are the useful resources actually found:

| Part | Resource | Status and applicability |
|---|---|---|
| ALPS EC11E15244G1 | [ALPS product page](https://tech.alpsalpine.com/e/products/detail/EC11E15244G1/) | Exact dimension/mounting drawings saved. Page offers 3D CAD under member access; model not downloaded or checked. |
| ALPS RKJXV122400R | [SnapMagic symbol/footprint/3D page](https://www.snapeda.com/parts/RKJXV122400R/ALPS/view-part/) | Page advertises CAD resources but previews/pinout were unavailable. No import or geometry validation performed; not a verified 4005 model. ALPS also offers a member CAD route for the R part. |
| Waveshare ESP32-S3-Zero | [Official hardware resources](https://docs.waveshare.com/ESP32-S3-Zero/Resources-And-Documents) · [saved STEP](research-2026-09-09/Waveshare-ESP32-S3-Zero-v2.stp) · [saved dimension drawing](research-2026-09-09/Waveshare-ESP32-S3-Zero-dimensions.jpg) | **Manufacturer STEP downloaded**, file header checked. Drawing is 18 × 23.5 mm, 2.54 mm side pitch, 15.24 mm row spacing. These files belong to Waveshare's board; no generic SuperMini match is established. |
| Adafruit MAX98357A, product 3006 | [Module drawings and Eagle files](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/downloads) · [STEP repository](https://github.com/adafruit/Adafruit_CAD_Parts/tree/main/3006%20MAX98357) | Manufacturer module CAD available. Applies to Adafruit's board, not automatically K04's clone. |
| PCM5102A chip | [TI product/CAD page](https://www.ti.com/product/PCM5102A) | Ultra Librarian link for IC symbol, package footprint and 3D. A carrier for K03 needs a module footprint instead. |
| MAX98357A chip | [Analog Devices product/CAD page](https://www.analog.com/en/products/MAX98357A.html) | Ultra Librarian/SamacSys resources by package. Select the exact package only for a future board using the bare IC. |
| Other candidate boards | Exact module libraries not verified | Draw the module outline, header pads, mounting holes and keepouts from its supplier drawing after the SKU is fixed. |

DipTrace supports importing other CAD formats. SamacSys explains that its former DipTrace downloads used PADS files and that these now reside in its PADS folder. Treat this as an **import route**, not native DipTrace availability. Follow the installed version's documentation, import footprint and component together, then check pin numbers, hole sizes and model alignment. [SamacSys explanation](https://www.samacsys.com/supporting-diptrace/) · [DipTrace import documentation](https://www.diptrace.com/support/tutorials/help_html/).

## Other parts needed to finish ten kits

These quantities are procurement allowances where the carrier has not yet been designed. Search links are explicitly **searches**, not checked product listings. Shipping and drawings remain open for these items too.

| Item | Quantity for ten kits | Buying specification / link |
|---|---|---|
| Encoder knobs | **10**, one 10-pack | [10-piece listing](https://www.aliexpress.com/item/33003636792.html) · [listing evidence](https://alitools.io/en/showcase/10pcs-6mm-shaft-hole-amplifier-knob-for-encoder-potentiometer-knobs-33003636792). Match the selected 6 mm shaft profile, bore depth and push clearance; obtain outside dimensions. |
| Joystick caps | **10**, if not supplied | Match the exact ALPS lever drawing. [AliExpress search](https://www.aliexpress.com/w/wholesale-10pcs-joystick-thumbstick-cap.html). Do not assume PS4/PS5/Xbox caps are interchangeable. |
| Male header strips | Budget **20 × 1×40**, 2.54 mm | Two 10-packs; deduct headers included with modules. [Search](https://www.aliexpress.com/w/wholesale-10pcs-1x40-2.54-male-header.html). Final cut lengths depend on selected boards. |
| Female socket strips | Budget **20 × 1×40**, 2.54 mm | Two [10-piece lots](https://www.aliexpress.com/item/1005013054690432.html) · [listing evidence](https://www.pricearchive.org/aliexpress.com/item/1005013054690432). Confirm socket height/drawing. Cutting ordinary sockets may sacrifice a position; pin budget is provisional. |
| USB data cables | **10** | USB-A to USB-C with USB 2.0 data conductors; choose a stated length such as 0.5 m. [Search](https://www.aliexpress.com/w/wholesale-10pcs-usb-a-usb-c-data-cable.html). Charge-only cables cannot flash or export files. |
| Power supply | **10 if each kit needs one** | Regulated 5 V supply; initial engineering allowance 2 A per kit, subject to power-path design and measured load. [Search](https://www.aliexpress.com/w/wholesale-10pcs-5v-2a-usb-power-supply.html). Plug type and destination remain open. |
| Speaker connectors/leads | **10 mating pairs** if detachable | Choose one exact connector, e.g. genuine JST-PH 2 mm, with a supplier drawing and adequate current rating. [Search](https://www.aliexpress.com/w/wholesale-10sets-jst-ph-2.0-2pin-wire.html). Terminal blocks supplied on the amp can remove this requirement. |
| Headphone sockets | **10 if absent from chosen headphone amp** | 3.5 mm stereo TRS socket. [Search](https://www.aliexpress.com/w/wholesale-10pcs-3.5mm-stereo-pcb-jack.html). Select exact part and pinout before drawing PCB; the DAC's existing socket remains line-out unless rerouted. |
| Internal wire/harnesses | **10 sets**, final lengths pending | Short signal wiring plus suitably sized power/speaker wiring. [Search](https://www.aliexpress.com/w/wholesale-10pcs-dupont-jumper-wire-set.html). Bare controls mount on the carrier; jumpers alone do not mount them. |
| Carrier PCBs | **10 assembled carriers** | **Not ready to order.** The repository has no finished carrier layout/manufacturing files. Bare control footprints, sockets, power distribution and mounting must be designed first. Final carrier resistors/capacitors/connectors require its schematic BOM. |
| I2C pull-up resistors | Allow **20 × 4.7 kΩ**; buy a 100-pack | Only populate if needed after checking module pull-ups. [Search](https://www.aliexpress.com/w/wholesale-100pcs-4.7k-resistor.html). Package is a carrier-design decision. |
| Supply/ADC/filter passives | **TBD by carrier schematic** | Reserve decoupling and bulk capacitance, and control filtering as required. Do not order an invented final value/package list before the power and carrier circuits exist. |
| Enclosures/lids | **10 printed pairs** | Existing [3D designs](../3d/README.md) require the final part dimensions. Select handheld/ocarina/cup before manufacturing. |
| Screws and mounting hardware | At least **40 lid screws** for handheld; allow a 100-pack | Handheld CAD has four lid screws per kit; module and carrier fixings are additional. M2-family hardware is the current intent, but length/type depend on final bosses. [Search](https://www.aliexpress.com/w/wholesale-100pcs-m2-self-tapping-screws.html). |
| Microphone/speaker gasket material | **10 cut sets** | Cut foam/gasket pieces to the actual mic port and speaker rim. Enclosure-dependent; protect the microphone opening from blockage. |

No external microSD card, SD module or crystal is specified for the current module build: firmware stores songs in the ESP32's onboard flash, and the modules provide their chip support circuits.

## Battery operation needs a completed power design

The enclosure contains placeholders for a 1S battery and TP4056 board, but the repository does not define a finished battery power path. The complete portable version additionally needs **10 protected 1S cells**, **10 suitable charging/power-path assemblies**, **10 5 V converters if not integrated**, **10 switches**, and matching battery harnesses. Battery size/capacity, charge current and connector polarity must be fixed together.

A TP4056 module alone does not supply a regulated 5 V rail or establish load sharing. Treat battery, charger and boost modules as **design holds**, not an approved three-item shopping recipe. Charge-while-playing also needs deliberate power-path support. [Microchip explanation of load sharing and power-path management](https://ww1.microchip.com/downloads/en/AppNotes/01149c.pdf).

Search leads for later: [protected 1S LiPo lots](https://www.aliexpress.com/w/wholesale-10pcs-3.7v-lipo-battery-protected.html) · [charger/power-path modules](https://www.aliexpress.com/w/wholesale-lipo-power-path-charger-5v-boost.html) · [switch lots](https://www.aliexpress.com/w/wholesale-10pcs-slide-switch.html). None has been qualified for shipping, documentation or this power budget.

## Documented alternatives if generic-board drawings cannot be obtained

These help meet the mechanical-documentation requirement. **Ten-piece lots and free shipping have not been established for these alternatives.** Substitution changes the carrier/enclosure design.

| Function | Documented part | Useful evidence |
|---|---|---|
| MCU | [Waveshare ESP32-S3-Zero](https://www.waveshare.com/product/esp32-s3-zero.htm) | 4 MB flash + 2 MB PSRAM; manufacturer schematic, dimension drawing and STEP. A firmware-compatible candidate by chip configuration, subject to pinout/revision and hardware checks. |
| Speaker amp | [Adafruit MAX98357A, 3006](https://www.adafruit.com/product/3006) | 19.4 × 17.8 × 3.0 mm product dimensions; dimensioned fab print, schematic, Eagle files and STEP. Account separately for fitted headers/terminal blocks. |
| IMU | [Adafruit MPU6050, 3886](https://www.adafruit.com/product/3886) | 26.0 × 17.8 × 4.6 mm; [fab print, schematic and Eagle files](https://learn.adafruit.com/mpu6050-6-dof-accelerometer-and-gyro/downloads). Different PCB from GY-521. |
| Distance | [Pololu VL53L0X carrier, 2490](https://www.pololu.com/product/2490) | Approximately 13 × 18 × 2 mm without headers; regulator, I2C level shifting and mounting information. |
| OLED | [Joy-IT SBC-OLED01 documentation](https://www.joy-it.net/files/files/Produkte/SBC-OLED01/SBC-OLED01_Datasheet_2022-01-06.pdf) | SSD1306 128×64; specified 27.5 × 27.7 × 11.2 mm. Confirm exact revision and full mounting drawing when sourcing. |
| Speaker | [Same Sky CMS-4012-34L200-X7](https://www.sameskydevices.com/product/audio/speakers/miniature-%2810-mm~40-mm%29/cms-4012-34l200-x7) | 40 × 40 × 12 mm, 4 Ω, 3 W nominal; manufacturer's datasheet. Its 12 mm depth differs substantially from the current 5.5 mm enclosure placeholder. |

## Build findings that affect the order

These findings come from the repository's README, firmware and enclosure files, checked on the research date:

- **MPU6050 is the intended motion sensor.** It contains a gyro and accelerometer, but `firmware/main/imu.c` currently derives tilt from accelerometer readings only. No separate gyroscope is needed for this implementation.
- **VL53L0X is the implemented distance sensor.** Although the pin-map document mentions VL53L1X too, `firmware/main/tof.c` uses VL53L0X registers and model ID.
- **The joystick enclosure model is still KY-023-style.** `3d/parts.scad` explicitly labels all its sizes as typical/unmeasured; it cannot establish fit for the bare ALPS joystick or a supplier's modules.
- **Headphones require additional hardware.** TI specifies PCM5102A line loads down to 1 kΩ; ordinary 16–64 Ω headphones need a suitable amplifier. K11 and its socket/coupling requirements therefore remain in the complete-kit scope. [TI specification](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf).
- **Power documentation conflicts.** The pin-map introduction says all modules use 3V3, while its PCM5102A section specifies 5 V input to the regulator-equipped DAC board. Resolve this against actual board schematics; do not power a bare PCM5102A chip with 5 V. Budget the speaker amplifier separately from the tiny board's 3.3 V regulator. MAX98357A's approximately 3 W capability assumes the appropriate 5 V supply/load conditions. [TI DAC documentation](https://www.ti.com/product/PCM5102A) · [Analog Devices amplifier documentation](https://www.analog.com/en/products/MAX98357A.html).

## Information needed to qualify the shortlist

1. Delivery **country and postcode**, and the intended meaning of “AliExpress special” if it is not Choice.
2. Confirmation that the target is **ten complete kits**, with only the encoder/joystick bare.
3. A readable selected-variant listing/cart view for the promising products, showing pack size and shipping. Exact supplier drawings can then be matched against the part and board photos.
4. Carrier, headphone and portable-power decisions before the remaining manufacturing BOM can be finalized.

Seller message you can copy (draft only; not sent):

> I need one lot of 10 identical pieces for a custom PCB. Please confirm the exact manufacturer part number and PCB revision, and provide a dimensioned drawing including all pin/hole positions and total height. For the encoder and joystick I require the bare component, without a breakout board. Please confirm which caps, nuts, headers and connectors are included, and whether a schematic symbol, PCB footprint or STEP model is available for this exact item.

The companion [CSV](purchasing-kit.csv) separates required quantity, advertised pack size, dimensional-document status and verification fields. The original placeholder `parts/bom.csv` has been preserved.
