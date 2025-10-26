#pragma once

// Pin assignments for the DFRobot Beetle ESP32-C6 Mini board.
namespace board_pins {
namespace dfrobot_beetle_esp32c6_mini {

constexpr int FLASH_SCK      = 23;
constexpr int FLASH_MOSI     = 22;
constexpr int FLASH_MISO     = 21;
constexpr int FLASH_CS       = 7;

constexpr int PERIPH_ENABLE_PIN  = -1;
constexpr int LED_PIN            = -1;

constexpr int UART_TX_PIN        = 16;
constexpr int UART_RX_PIN        = 17;

constexpr int I2C_SDA_PIN        = 19;
constexpr int I2C_SCL_PIN        = 20;

constexpr int GPIO_A3_PIN        = 4;
constexpr int GPIO_A4_PIN        = 5;
constexpr int GPIO_D12_PIN       = 6;

}  // namespace dfrobot_beetle_esp32c6_mini
}  // namespace board_pins
