# DFRobot Beetle ESP32-C6 Mini Pin Map

| GPIO | Arduino Alias | Label | Notes               |
| ---- | ------------- | ----- | ------------------- |
| 4    | A3            | A3    | GPIO / ADC          |
| 5    | A4            | A4    | GPIO / ADC          |
| 6    | D12           | D12   | GPIO                |
| 7    | D11           | D11   | Suggested SPI CS    |
| 16   | —             | TX    | UART TX             |
| 17   | —             | RX    | UART RX             |
| 19   | —             | SDA   | I²C SDA             |
| 20   | —             | SCL   | I²C SCL             |
| 21   | —             | MISO  | SPI MISO            |
| 22   | —             | MOSI  | SPI MOSI            |
| 23   | —             | SCK   | SPI SCK             |

Power pads: 3V3 (left top), VIN (right top), BAT (right top) and GND (both sides).

The companion header `pins_dfrobot_beetle_esp32c6_mini.h` exposes these
assignments under the `board_pins::dfrobot_beetle_esp32c6_mini` namespace.

