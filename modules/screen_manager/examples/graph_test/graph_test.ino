#include <Wire.h>
#include <U8g2lib.h>
#include "../../../dfrobot_beetle_esp32c6_mini/pins_dfrobot_beetle_esp32c6_mini.h"

using board_pins::dfrobot_beetle_esp32c6_mini::I2C_SCL_PIN;
using board_pins::dfrobot_beetle_esp32c6_mini::I2C_SDA_PIN;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);

constexpr uint8_t kWidth = 128;
constexpr uint8_t kHeight = 64;
constexpr uint8_t kGraphLeft = 8;
constexpr uint8_t kGraphRight = (kWidth / 2);
constexpr uint8_t kGraphTop = 10;
constexpr uint8_t kGraphBottom = (kHeight / 2);
constexpr uint8_t kGraphWidth = kGraphRight - kGraphLeft;
constexpr uint8_t kGraphHeight = kGraphBottom - kGraphTop;
constexpr uint8_t kSampleCount = 48;

float samples[kSampleCount];
uint8_t head = 0;

void pushSample(float value) {
  samples[head] = value;
  head = (head + 1) % kSampleCount;
}

float minSample() {
  float minVal = samples[0];
  for (uint8_t i = 1; i < kSampleCount; ++i) {
    if (samples[i] < minVal) minVal = samples[i];
  }
  return minVal;
}

float maxSample() {
  float maxVal = samples[0];
  for (uint8_t i = 1; i < kSampleCount; ++i) {
    if (samples[i] > maxVal) maxVal = samples[i];
  }
  return maxVal;
}

void drawGraph() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(4, 7, "Graph test - PM2.5");

  u8g2.drawFrame(kGraphLeft - 1, kGraphTop - 1, kGraphWidth + 2, kGraphHeight + 2);

  float minVal = minSample();
  float maxVal = maxSample();
  if (maxVal - minVal < 0.5f) {
    maxVal = minVal + 0.5f;
  }

  float lastX = kGraphLeft;
  float lastY = kGraphBottom - ((samples[(head) % kSampleCount] - minVal) / (maxVal - minVal)) * kGraphHeight;

  for (uint8_t i = 1; i < kSampleCount; ++i) {
    uint8_t index = (head + i) % kSampleCount;
    float value = samples[index];
    float x = kGraphLeft + (i * (kGraphWidth / (float)(kSampleCount - 1)));
    float y = kGraphBottom - ((value - minVal) / (maxVal - minVal)) * kGraphHeight;
    u8g2.drawLine((int)lastX, (int)lastY, (int)x, (int)y);
    lastX = x;
    lastY = y;
  }

  char labelBuf[24];
  snprintf(labelBuf, sizeof(labelBuf), "min %.1f | max %.1f", minVal, maxVal);
  u8g2.drawStr(4, kHeight - 2, labelBuf);

  u8g2.sendBuffer();
}

void setup() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();

  for (uint8_t i = 0; i < kSampleCount; ++i) {
    samples[i] = 12.0f;
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(10, 32, "Initializing graph...");
  u8g2.sendBuffer();
}

void loop() {
  float t = millis() / 1200.0f;
  float base = 12.0f + 6.0f * sinf(t * 0.25f);
  float noise = 1.5f * sinf(t * 1.7f) + 0.8f * cosf(t * 0.9f);
  pushSample(base + noise);

  drawGraph();

  delay(250);
}
