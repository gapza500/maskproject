#include <Arduino.h>
#include <Wire.h>
#include <GyverOLED.h>

GyverOLED<SSH1106_128x64> oled;

// WiFi icon bitmap (16x16 pixels)
const uint8_t wifiIcon16x16[] PROGMEM = {
  0b00000000, 0b00000000,
  0b00011111, 0b11100000,
  0b00111111, 0b11110000,
  0b01110000, 0b00011000,
  0b11100111, 0b11101100,
  0b11001111, 0b11100110,
  0b00011100, 0b00110000,
  0b00111000, 0b00011000,
  0b01110000, 0b00001100,
  0b01100000, 0b00000100,
  0b00000000, 0b00000000,
  0b00001100, 0b00110000,
  0b00000110, 0b01100000,
  0b00000011, 0b11000000,
  0b00000001, 0b10000000,
  0b00000000, 0b00000000
};

void setup() {
  Wire.begin();
  oled.init();
  oled.clear();

  // Draw WiFi icon at position (x=100, y=0)
  oled.drawBitmap(100, 0, wifiIcon16x16, 16, 16, 1);

  oled.update();
}

void loop() {
}
