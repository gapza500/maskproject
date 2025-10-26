# Cytron Maker Feather AIoT S3 Pin Map

| GPIO | Alias | Function / Notes        |
| ---- | ----- | ----------------------- |
| 0    | BOOT  | Boot strapping, no PWM  |
| 2    | ALED  | `LED_BUILTIN`, PWM-capable |
| 3    | BTN   | `BUTTON`, PWM-capable   |
| 4    | A4    | GPIO / ADC / Touch      |
| 5    | A3    | GPIO / ADC / Touch      |
| 6    | A2    | GPIO / ADC / Touch      |
| 7    | A5    | SPI CS (`SS`)           |
| 8    | A7    | SPI MOSI                |
| 9    | A1    | GPIO / ADC / Touch      |
| 10   | A0    | GPIO / ADC / Touch      |
| 11   | VP_EN | Peripheral rail enable  |
| 12   |       | Buzzer output           |
| 13   | A12   | VIN/VBAT monitor        |
| 14   | A11   | GPIO / Touch            |
| 15   | A10   | UART TX                 |
| 16   | A9    | UART RX                 |
| 17   | A6    | SPI SCK                 |
| 18   | A8    | SPI MISO                |
| 41   |       | I²C SCL                 |
| 42   |       | I²C SDA                 |
| 46   |       | RGB LED (strapping)     |

The accompanying `pins_cytron_maker_feather_aiot_s3.h` header exposes these
assignments via the `board_pins::cytron_maker_feather_aiot_s3` namespace for use
in firmware modules.

