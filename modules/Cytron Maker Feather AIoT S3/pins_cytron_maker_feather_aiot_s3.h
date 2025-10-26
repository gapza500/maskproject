#pragma once

// Pin assignments for the Cytron Maker Feather AIoT S3 board.
// Source: modules/Cytron Maker Feather AIoT S3/README.md

namespace board_pins {
namespace cytron_maker_feather_aiot_s3 {

// SPI flash (external W25Q128)
constexpr int FLASH_SCK  = 17;
constexpr int FLASH_MISO = 18;
constexpr int FLASH_MOSI = 8;
constexpr int FLASH_CS   = 7;

// Power / peripherals
constexpr int PERIPH_ENABLE = 11;  // VPERIPH_EN rail switch, -1 if unused

// User I/O
constexpr int LED        = 2;
constexpr int BUTTON     = 3;
constexpr int BUZZER     = 12;
constexpr int VBAT_MON   = 13;     // VIN/VBAT monitor divider

// RTC / I2C default pins
constexpr int I2C_SDA    = 42;
constexpr int I2C_SCL    = 41;

// UART
constexpr int UART_TX    = 15;
constexpr int UART_RX    = 16;

}  // namespace cytron_maker_feather_aiot_s3
}  // namespace board_pins

