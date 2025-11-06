# ESP32 DevKit V1 Pin Map

Include the header with:

```cpp
#include "modules/esp32_devkit_v1/pins_esp32_devkit_v1.h"
```

The table below lists the common pin capabilities for quick reference.

| GPIO | ADC     | Touch  | DAC  | I2C     | SPI                      | UART               | Other / Notes                              |
| ---- | ------- | ------ | ---- | ------- | ------------------------ | ------------------ | ------------------------------------------ |
| EN   |         |        |      |         |                          |                    | Enable (reset) pin                         |
| 36   | ADC1_0  |        |      |         |                          |                    | Input-only (SensVP)                        |
| 39   | ADC1_3  |        |      |         |                          |                    | Input-only (SensVN)                        |
| 34   | ADC1_6  |        |      |         |                          |                    | Input-only                                 |
| 35   | ADC1_7  |        |      |         |                          |                    | Input-only                                 |
| 32   | ADC1_4  | Touch9 |      |         |                          |                    | RTC GPIO9 / XTAL32P                        |
| 33   | ADC1_5  | Touch8 |      |         |                          |                    | RTC GPIO8 / XTAL32N                        |
| 25   | ADC2_8  |        | DAC1 |         |                          |                    |                                            |
| 26   | ADC2_9  |        | DAC2 |         |                          |                    |                                            |
| 27   | ADC2_7  | Touch7 |      |         |                          |                    | RTC GPIO17                                 |
| 14   | ADC2_6  | Touch6 |      |         | HSPI_CLK                 |                    | RTC GPIO16                                 |
| 12   | ADC2_5  | Touch5 |      |         | HSPI_Q                   |                    | RTC GPIO15 (strapping)                     |
| 13   | ADC2_4  | Touch4 |      |         | HSPI_ID                  |                    | RTC GPIO14                                 |
| 23   |         |        |      |         | VSPI_MOSI (V_SPI_D)      |                    |                                            |
| 22   |         |        |      | I2C_SCL | VSPI_WP                  |                    | Also RTS0 on some boards                   |
| 1    |         |        |      |         |                          | UART0_TX (TXD0)    | Boot log pin                               |
| 3    |         |        |      |         |                          | UART0_RX (RXD0)    | Boot mode pin                              |
| 21   |         |        |      | I2C_SDA | VSPI_HD                  |                    |                                            |
| 19   |         |        |      |         | VSPI_MISO (V_SPI_Q)      |                    | Also CTS0 on some boards                   |
| 18   |         |        |      |         | VSPI_SCK                 |                    |                                            |
| 5    |         |        |      |         | VSPI_CS0                 |                    | Often used as SS (strapping)               |
| 17   |         |        |      |         |                          | UART2_TX (TXD2)    |                                            |
| 16   |         |        |      |         |                          | UART2_RX (RXD2)    |                                            |
| 4    | ADC2_0  | Touch0 |      |         | HSPI_HD                  |                    | RTC GPIO10                                 |
| 2    | ADC2_2  | Touch2 |      |         | HSPI_WP0                 |                    | RTC GPIO12 (strapping, onboard LED option) |
| 15   | ADC2_3  | Touch3 |      |         | HSPI_CS0                 |                    | RTC GPIO13 (strapping)                     |
| VIN  |         |        |      |         |                          |                    | 5-12V input (board-specific)               |
| 3V3  |         |        |      |         |                          |                    | Regulated 3.3V output                      |
| GND  |         |        |      |         |                          |                    | Ground                                     |
