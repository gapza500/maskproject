#pragma once

// Pin assignments for the Cytron Maker Feather AIoT S3 board.
namespace board_pins {
namespace cytron_maker_feather_aiot_s3 {

constexpr int FLASH_SCK  = 17;
constexpr int FLASH_MISO = 18;
constexpr int FLASH_MOSI = 8;
constexpr int FLASH_CS   = 7;

constexpr int PERIPH_ENABLE_PIN = 11;
constexpr int LED_PIN           = 2;
constexpr int BUTTON_PIN        = 3;
constexpr int BUZZER_PIN        = 12;
constexpr int VBAT_MON_PIN      = 13;

constexpr int I2C_SDA_PIN = 42;
constexpr int I2C_SCL_PIN = 41;

constexpr int UART_TX_PIN = 15;
constexpr int UART_RX_PIN = 16;

}  // namespace cytron_maker_feather_aiot_s3
}  // namespace board_pins
