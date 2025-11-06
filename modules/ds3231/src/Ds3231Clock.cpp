#include "../include/Ds3231Clock.h"

RTC_DS3231 Ds3231Clock::_defaultRtc;

Ds3231Clock::Ds3231Clock(RTC_DS3231& rtc) : _rtc(rtc) {}

bool Ds3231Clock::begin(TwoWire& wire) {
  if (!_rtc.begin(&wire)) {
    _initialized = false;
    return false;
  }
  _initialized = true;
  _shadowUnix = _rtc.now().unixtime();
  _shadowTempC = _rtc.getTemperature();
  _hasSimulatedTemp = false;
  return true;
}

bool Ds3231Clock::lostPower() const {
  if (!_initialized) return true;
  return _rtc.lostPower();
}

bool Ds3231Clock::running() const {
  if (!_initialized) return false;
  return !_rtc.lostPower();
}

DateTime Ds3231Clock::now() const {
  if (!_initialized) return DateTime();
  return _rtc.now();
}

void Ds3231Clock::adjust(const DateTime& dt) {
  if (!_initialized) return;
  _rtc.adjust(dt);
  _shadowUnix = dt.unixtime();
}

void Ds3231Clock::setUnixTime(uint32_t epoch) {
  _shadowUnix = epoch;
  if (_initialized) {
    // avoid repeatedly adjusting hardware unless explicitly desired
  }
}

void Ds3231Clock::setTemperatureC(float tempC) {
  _shadowTempC = tempC;
  _hasSimulatedTemp = true;
}

uint32_t Ds3231Clock::unixTime() const {
  if (_initialized) {
    return _rtc.now().unixtime();
  }
  return _shadowUnix;
}

float Ds3231Clock::temperatureC() const {
  if (_hasSimulatedTemp || !_initialized) {
    return _shadowTempC;
  }
  return _rtc.getTemperature();
}
