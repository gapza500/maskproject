# Getting Started

This guide walks through wiring, installing dependencies, and flashing the
baseline sketch for miniFlashDataBase v2.0.

## Hardware

- ESP32-S3 or ESP32-C6 board (tested with Cytron Maker Feather AIoT S3 and
  DFRobot Beetle ESP32-C6 mini).
- Winbond W25Q128 (or compatible) SPI flash connected to VSPI/FSPI.
- DS3231 RTC module on the I2C bus.
- Optional: MAX17048 fuel gauge for battery guard events.

### Pinouts

Refer to `modules/cytron_maker_feather_aiot_s3/README.md` and
`modules/dfrobot_beetle_esp32c6_mini/README.md` for exact mappings. At minimum:

```
Flash  CS  -> GPIO7 (S3) / GPIO7 (C6)
Flash  SCK -> GPIO17 (S3) / GPIO23 (C6)
Flash MOSI -> GPIO8  (S3) / GPIO22 (C6)
Flash MISO -> GPIO18 (S3) / GPIO21 (C6)
RTC   SDA  -> GPIO42 (S3) / GPIO8  (C6)
RTC   SCL  -> GPIO41 (S3) / GPIO9  (C6)
```

Enable any board-specific power rails (e.g. `PERIPH_ENABLE_PIN` on the Cytron).

## Software

1. Install the **Arduino IDE** and the **ESP32** board support package
   (v3.0+).
2. Install libraries from the Library Manager:
   - `RTClib`
   - `Preferences` (bundled with ESP32 core)
   - `ArduinoJson` (for the examples)
3. Clone or copy this repository into your Arduino sketchbook under
   `libraries/miniFlashDataBase`.

## Baseline Sketch

The production reference sketch lives at
`labs/FlashDatabase/miniFlashDataBase_v2_0/miniFlashDataBase_v2_0.ino`. Steps:

1. Open the sketch in Arduino IDE.
2. Select your board (e.g. **Adafruit Feather ESP32-S3**) and serial port.
3. Update the board namespace block if you use a board other than the two
   defaults.
4. Flash the sketch. Serial output at 115200 should show:
   - Flash unprotect log
   - RTC confirmation
   - `FlashLogger v1.8 ready...`
   - Shell prompt with supported commands.

You can now append sensor JSON payloads or drive the built-in shell (`help` for
commands).

## Next Steps

- Run the regression harnesses under `labs/FlashDatabase/miniFlashDataBase_v1_96_tests`
  and `...v1_99_tests` to validate pagination/predicates.
- Browse the `examples/` directory for additional integration patterns.

