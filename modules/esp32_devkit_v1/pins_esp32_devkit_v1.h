#pragma once

// Pin assignments for the ESP32 DevKit V1 module.
namespace board_pins {
namespace esp32_devkit_v1 {

constexpr int LED_PIN             = 2;   // Onboard LED on many DevKit V1 boards

constexpr int I2C_SDA_PIN         = 21;
constexpr int I2C_SCL_PIN         = 22;

constexpr int VSPI_MOSI_PIN       = 23;
constexpr int VSPI_MISO_PIN       = 19;
constexpr int VSPI_SCK_PIN        = 18;
constexpr int VSPI_CS0_PIN        = 5;
constexpr int VSPI_WP_PIN         = 22;
constexpr int VSPI_HD_PIN         = 21;

constexpr int HSPI_MOSI_PIN       = 13;
constexpr int HSPI_MISO_PIN       = 12;
constexpr int HSPI_SCK_PIN        = 14;
constexpr int HSPI_CS0_PIN        = 15;
constexpr int HSPI_WP_PIN         = 2;
constexpr int HSPI_HD_PIN         = 4;

constexpr int UART0_TX_PIN        = 1;
constexpr int UART0_RX_PIN        = 3;
constexpr int UART2_TX_PIN        = 17;
constexpr int UART2_RX_PIN        = 16;

constexpr int DAC1_PIN            = 25;
constexpr int DAC2_PIN            = 26;

constexpr int ADC1_CH0_PIN        = 36;  // Input only
constexpr int ADC1_CH3_PIN        = 39;  // Input only
constexpr int ADC1_CH6_PIN        = 34;  // Input only
constexpr int ADC1_CH7_PIN        = 35;  // Input only

constexpr int TOUCH0_PIN          = 4;
constexpr int TOUCH2_PIN          = 2;
constexpr int TOUCH3_PIN          = 15;
constexpr int TOUCH4_PIN          = 13;
constexpr int TOUCH5_PIN          = 12;
constexpr int TOUCH6_PIN          = 14;
constexpr int TOUCH7_PIN          = 27;
constexpr int TOUCH8_PIN          = 33;
constexpr int TOUCH9_PIN          = 32;

}  // namespace esp32_devkit_v1
}  // namespace board_pins

