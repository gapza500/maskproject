#include "../../include/device_status/providers/Ds3231StatusProvider.h"
#include "../../include/device_status/GlobalTime.h"

#include <Wire.h>
#include <stdarg.h>

namespace device_status {

Ds3231StatusProvider::Ds3231StatusProvider(Ds3231Clock& rtc) : _rtc(rtc) {
  setDetails("not initialised");
}

bool Ds3231StatusProvider::begin() {
  _present = _rtc.begin(Wire);
  if (!_present) {
    setDetails("rtc not found");
    return false;
  }

  if (_rtc.lostPower()) {
    _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    setDetails("rtc recovered after power loss");
  } else {
    setDetails("rtc ok");
  }
  return true;
}

DeviceStatusCode Ds3231StatusProvider::refresh(DeviceStatusReport& report) {
  if (!_present) {
    report.rtc = STATUS_NOT_FOUND;
    return report.rtc;
  }

  bool lost = _rtc.lostPower();
  bool running = _rtc.running();
  DateTime now = _rtc.now();
  float temp = _rtc.temperatureC();

  GlobalTime::update(now);

  report.rtcData.lostPower = lost;
  report.rtcData.running = running;
  report.rtcData.temperatureC = temp;
  report.rtcData.unixTime = now.unixtime();
  report.rtcData.valid = !lost;
  report.rtcData.batteryLow = lost;

  if (!running) {
    report.rtc = STATUS_ERROR;
    setDetails("oscillator stopped");
    return report.rtc;
  }

  if (lost) {
    report.rtc = STATUS_WARN;
    setDetails("rtc lost power");
  } else {
    report.rtc = STATUS_OK;
    setDetails("rtc ok (%lu)", static_cast<unsigned long>(now.unixtime()));
  }
  return report.rtc;
}

void Ds3231StatusProvider::setDetails(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(_details, sizeof(_details), fmt, args);
  va_end(args);
}

}  // namespace device_status
