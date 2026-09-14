# Minimal audio kit — 7 parts

**Your carrier PCB + seven purchased parts per kit.** For ten kits: buy ten of each, preferably one 10-piece lot. Scope confirmed 9 September 2026: **joystick on a board with pins; encoder bare**.

| # | Part | Required version | AliExpress candidate |
|---|---|---|---|
| 1 | ESP32-S3 SuperMini | ESP32-S3FH4R2, 4 MB flash / 2 MB PSRAM, USB-C | [1–10 pcs](https://www.aliexpress.com/item/1005012191736844.html) |
| 2 | PCM5102A I2S DAC | **Purple board**, with audio jack | [1–10 pcs](https://www.aliexpress.com/item/1005010369195534.html) |
| 3 | Analog joystick | **KY-023-style board with fitted pins and cap**; X, Y and push button | [1/5/10 pcs](https://www.aliexpress.com/item/1005008298413198.html) |
| 4 | Rotary encoder | **Bare EC11 with push button**, vertical shaft | [10 pcs](https://www.aliexpress.com/item/1005005239756119.html) |
| 5 | I2S microphone | **Small round six-pin board**; INMP441 assumed from this project | [1/5/10 pcs](https://www.aliexpress.com/item/1005009006830769.html) |
| 6 | 0.96-inch OLED | SSD1306, 128×64, **I2C SDA/SCL**, four pins | [10 pcs](https://www.aliexpress.com/item/1005008169257099.html) |
| 7 | Gyroscope / accelerometer | **MPU6050 GY-521 board**, I2C | [10 pcs](https://www.aliexpress.com/item/1005011896641210.html) |

**Ordering status:** these are listing leads. Select the 10-piece variant where available. Free shipping/Choice, fitted headers, the purple DAC version, round microphone version and exact board drawings remain **unverified**. Country/postcode and readable seller details are still needed. The ten-kit batch is **70 purchased parts + 10 of your carrier PCBs**.

**Documentation and CAD:** [PCM5102A datasheet](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf) · [INMP441 datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf) · [MPU6050 datasheet](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf) · [KY-023 module manual/dimensions reference](https://www.joy-it.net/en/products/COM-KY023JM) · [SSD1306 OLED module dimensions reference](https://www.joy-it.net/files/files/Produkte/SBC-OLED01/SBC-OLED01_Datasheet_2022-01-06.pdf). Reference-board dimensions must be matched to the actual supplier's boards. Exact DipTrace libraries remain unverified; existing drawings and CAD leads are in the [detailed research](purchasing-kit.md).

**Carrier design:** use the actual module header layouts and bare encoder footprint. Keep joystick outputs and I2C pull-ups at 3.3 V. The purple DAC's input power and configuration straps depend on its revision. Its output is line level; if headphone drive is wanted, account for that in the carrier design. [TI DAC specification](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf).

Listing-title evidence: [ESP32](https://sv.pricearchive.org/aliexpress.com/item/1005012191736844) · [DAC](https://www.pricearchive.org/aliexpress.com/item/1005010369195534) · [joystick](https://ms.pricearchive.org/aliexpress.com/item/1005008298413198) · [encoder](https://sv.pricearchive.org/aliexpress.com/item/1005005239756119) · [mic](https://vi.pricearchive.org/aliexpress.com/item/1005009006830769) · [OLED](https://ro.pricearchive.org/aliexpress.com/item/1005008169257099) · [IMU](https://www.pricearchive.org/aliexpress.com/item/1005011896641210). These indexed records do not verify shipping or the selected variant.

[Editable seven-part CSV](minimal-kit.csv) · [Earlier expanded research](purchasing-kit.md)
