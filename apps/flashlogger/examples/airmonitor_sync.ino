#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

#include "FlashLogger.h"
#include "UploadHelpers.h"

#include "pins_cytron_maker_feather_aiot_s3.h"  // adjust board as needed

RTC_DS3231 rtc;
FlashLogger logger(board_pins::cytron_maker_feather_aiot_s3::FLASH_CS);

struct Measurement {
  float pm2_5;
  float temperature;
  float humidity;
  float battery;
};

Measurement readSensors() {
  Measurement m;
  m.pm2_5 = 10.0f + random(0, 50) / 10.0f;
  m.temperature = 25.0f + random(-15, 15) / 10.0f;
  m.humidity = 55.0f + random(-100, 100) / 10.0f;
  m.battery = 85.0f + random(-20, 5) / 10.0f;
  return m;
}

bool sendPayload(const char* payload, size_t len, const char* key, void* user) {
  Serial.print(F("[uplink] key="));
  Serial.print(key);
  Serial.print(F(" payload="));
  Serial.write(payload, len);
  Serial.println();
  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();

  FlashLoggerConfig cfg;
  cfg.rtc = &rtc;
  cfg.spi_cs_pin = board_pins::cytron_maker_feather_aiot_s3::FLASH_CS;
  cfg.spi_sck_pin = board_pins::cytron_maker_feather_aiot_s3::FLASH_SCK;
  cfg.spi_mosi_pin = board_pins::cytron_maker_feather_aiot_s3::FLASH_MOSI;
  cfg.spi_miso_pin = board_pins::cytron_maker_feather_aiot_s3::FLASH_MISO;
  cfg.spi_clock_hz = 8'000'000;
  cfg.totalSizeBytes = 16UL * 1024UL * 1024UL;
  cfg.csvColumns = "ts,temp,hum,pm2_5,bat";

  if (!logger.begin(cfg)) {
    Serial.println(F("FlashLogger init failed"));
    while (true) delay(1000);
  }

  logger.rescanAndRefresh(true, false);
}

void loop() {
  DateTime now = rtc.now();
  Measurement m = readSensors();

  String payload = String("{\"ts\":") + now.unixtime() +
                   ",\"temp\":" + m.temperature +
                   ",\"hum\":" + m.humidity +
                   ",\"pm2_5\":" + m.pm2_5 +
                   ",\"bat\":" + m.battery + "}";
  logger.append(payload);

  SyncCursor cur;
  if (logger.getCursor(cur)) {
    FlashLoggerUploadPolicy pol;
    pol.maxAttempts = 1;
    flashlogger_upload_ndjson(logger, cur, 5, sendPayload, nullptr, pol, nullptr);
  }

  delay(5000);
}
