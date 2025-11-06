#pragma once

#include <Arduino.h>
#include <RTClib.h>

class Ds3231Clock {
 public:
  explicit Ds3231Clock(RTC_DS3231& rtc = _defaultRtc);

  bool begin(TwoWire& wire = Wire);

  bool lostPower() const;
  bool running() const;

  DateTime now() const;
  void adjust(const DateTime& dt);
  void setUnixTime(uint32_t epoch);
  uint32_t unixTime() const;

  void setTemperatureC(float tempC);
  float temperatureC() const;

  RTC_DS3231& raw() { return _rtc; }
  const RTC_DS3231& raw() const { return _rtc; }

 private:
  static RTC_DS3231 _defaultRtc;
  RTC_DS3231& _rtc;
  bool _initialized = false;
  uint32_t _shadowUnix = 0;
  float _shadowTempC = NAN;
  bool _hasSimulatedTemp = false;
};
