#pragma once

// Pin assignments for the DFRobot Beetle ESP32-C6 Mini board.
// Source: modules/DFrobot beetle esp32 c6 mini/README.md

namespace board_pins {
namespace dfrobot_beetle_esp32c6_mini {

// SPI header
constexpr int FLASH_SCK      = 23;  // labeled SCK
constexpr int FLASH_MOSI     = 22;  // labeled MOSI
constexpr int FLASH_MISO     = 21;  // labeled MISO
constexpr int FLASH_CS       = 7;   // labeled D11 (use as CS)

// Power / peripherals
constexpr int PERIPH_ENABLE  = -1;  // no dedicated rail switch
constexpr int LED            = -1;  // external LED required

// UART
constexpr int UART_TX        = 16;
constexpr int UART_RX        = 17;

// I2C default pins
constexpr int I2C_SDA        = 19;
constexpr int I2C_SCL        = 20;

// General-purpose GPIO / ADC pads
constexpr int GPIO_A3        = 4;
constexpr int GPIO_A4        = 5;
constexpr int GPIO_D12       = 6;

}  // namespace dfrobot_beetle_esp32c6_mini
}  // namespace board_pins
