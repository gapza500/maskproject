#include "../../include/device_status/providers/Max17048StatusProvider.h"

#include <math.h>

namespace device_status {

Max17048StatusProvider::Max17048StatusProvider(Max17048& gauge, TwoWire& wire)
    : _gauge(gauge), _wire(wire) {
  setDetails("not initialised");
}

bool Max17048StatusProvider::begin() {
  _wire.begin();
  _present = _gauge.begin();
  if (!_present) {
    setDetails("not found on bus");
    return false;
  }
  setDetails("ok");
  return true;
}

DeviceStatusCode Max17048StatusProvider::refresh(DeviceStatusReport& report) {
  if (!_present) {
    report.battery = STATUS_NOT_FOUND;
    report.batteryData.dataValid = false;
    return report.battery;
  }

  float percent = _gauge.readPercent();
  float voltage = _gauge.readVoltage();
  bool percentValid = !isnan(percent);
  bool voltageValid = !isnan(voltage);

  report.batteryData.percent = percent;
  report.batteryData.voltage = voltage;
  report.batteryData.dataValid = percentValid || voltageValid;

  if (!percentValid && !voltageValid) {
    setDetails("read failed");
    report.battery = STATUS_ERROR;
    return report.battery;
  }

  if (percentValid && percent <= _errorPercent) {
    setDetails("battery critically low");
    report.battery = STATUS_ERROR;
  } else if (percentValid && percent <= _warnPercent) {
    setDetails("battery low");
    report.battery = STATUS_WARN;
  } else {
    setDetails("battery ok");
    report.battery = STATUS_OK;
  }
  return report.battery;
}

void Max17048StatusProvider::setDetails(const char* msg) {
  snprintf(_details, sizeof(_details), "%s", msg);
}

}  // namespace device_status
