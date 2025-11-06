#pragma once

#include <SensirionI2cSen66.h>
#include <Wire.h>

#include "../StatusProvider.h"

namespace device_status {

class Sen66StatusProvider : public StatusProvider {
 public:
  Sen66StatusProvider(SensirionI2cSen66& driver, TwoWire& wire, uint8_t address = 0x6B);

  const char* name() const override { return "sen66"; }
  bool begin() override;
  DeviceStatusCode refresh(DeviceStatusReport& report) override;
  const char* details() const override { return _details; }

 private:
  static DeviceStatusCode evaluateStatus(uint32_t raw);
  void setDetails(const char* msg);

  SensirionI2cSen66& _driver;
  TwoWire& _wire;
  const uint8_t _address;
  char _details[96];
  bool _started = false;
};

}  // namespace device_status
