#pragma once

#include <Arduino.h>
#include <math.h>

#include "DeviceStatusCodes.h"

namespace device_status {

struct Sen66Data {
  float pm10 = NAN;
  float pm4 = NAN;
  float pm25 = NAN;
  float pm1 = NAN;
  float humidity = NAN;
  float temperatureC = NAN;
  float vocIndex = NAN;
  float noxIndex = NAN;
  uint16_t co2ppm = 0;
  uint32_t rawStatus = 0;
  bool dataValid = false;
};

struct BatteryData {
  float percent = NAN;
  float voltage = NAN;
  bool dataValid = false;
};

struct RtcData {
  bool running = false;
  bool lostPower = true;
  float temperatureC = NAN;
  uint32_t unixTime = 0;
  bool valid = false;
  bool batteryLow = false;
};

struct OledData {
  bool initialised = false;
  bool lastCommandOk = false;
  uint8_t activeRows = 0;
};

struct FlashData {
  bool initialised = false;
  bool selfTestPassed = false;
  uint32_t lastError = 0;
  float totalMB = NAN;
  float usedMB = NAN;
  float freeMB = NAN;
  float usedPercent = NAN;
  float healthPercent = NAN;
  uint16_t estimatedDaysLeft = 0;
};

struct DeviceStatusReport {
  DeviceStatusCode sen66 = STATUS_NOT_FOUND;
  DeviceStatusCode battery = STATUS_NOT_FOUND;
  DeviceStatusCode rtc = STATUS_NOT_FOUND;
  DeviceStatusCode oled = STATUS_NOT_FOUND;
  DeviceStatusCode flash = STATUS_NOT_FOUND;

  Sen66Data sen66Data{};
  BatteryData batteryData{};
  RtcData rtcData{};
  OledData oledData{};
  FlashData flashData{};

  void reset() {
    *this = DeviceStatusReport();
  }
};

}  // namespace device_status
