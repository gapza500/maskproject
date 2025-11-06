#include "../include/device_status/DeviceStatusManager.h"
#include "../include/device_status/Report.h"

#include "../../../modules/test_support/Arduino.h"

#include <cassert>
#include <vector>

using device_status::DeviceStatusCode;
using device_status::DeviceStatusManager;
using device_status::DeviceStatusReport;
using device_status::StatusProvider;

class FakeProvider : public StatusProvider {
 public:
  FakeProvider(const char* name,
               DeviceStatusCode code,
               DeviceStatusCode DeviceStatusReport::*field,
               bool beginResult = true)
      : _name(name), _code(code), _field(field), _beginResult(beginResult) {}

  const char* name() const override { return _name; }

  bool begin() override {
    beginCalled = true;
    return _beginResult;
  }

  DeviceStatusCode refresh(DeviceStatusReport& report) override {
    report.*_field = _code;
    refreshCalls++;
    return _code;
  }

  const char* details() const override { return "fake"; }

  bool beginCalled = false;
  int refreshCalls = 0;

 private:
  const char* _name;
  DeviceStatusCode _code;
  DeviceStatusCode DeviceStatusReport::*_field;
  bool _beginResult;
};

int main() {
  DeviceStatusReport report;
  DeviceStatusManager manager(report);

  FakeProvider okSen("sen66", DeviceStatusCode::STATUS_OK, &DeviceStatusReport::sen66);
  FakeProvider warnBattery("battery", DeviceStatusCode::STATUS_WARN, &DeviceStatusReport::battery);
  FakeProvider badRtc("rtc", DeviceStatusCode::STATUS_ERROR, &DeviceStatusReport::rtc);
  FakeProvider missingOled("oled", DeviceStatusCode::STATUS_NOT_FOUND, &DeviceStatusReport::oled, false);

  assert(manager.addProvider(okSen));
  assert(manager.addProvider(warnBattery));
  assert(manager.addProvider(badRtc));
  assert(manager.addProvider(missingOled));

  bool initResult = manager.beginAll();
  assert(initResult == false);  // one provider failed begin()
  assert(okSen.beginCalled);
  assert(missingOled.beginCalled);

  manager.refreshAll();

  assert(report.sen66 == DeviceStatusCode::STATUS_OK);
  assert(report.battery == DeviceStatusCode::STATUS_WARN);
  assert(report.rtc == DeviceStatusCode::STATUS_ERROR);
  assert(report.oled == DeviceStatusCode::STATUS_NOT_FOUND);

  assert(okSen.refreshCalls == 1);
  assert(missingOled.refreshCalls == 1);

  return 0;
}
