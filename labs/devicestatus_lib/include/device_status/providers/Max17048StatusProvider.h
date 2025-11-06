#pragma once

#include <Wire.h>

#include "../StatusProvider.h"
#include "../../../../../modules/max17048/Max17048.h"

namespace device_status {

class Max17048StatusProvider : public StatusProvider {
 public:
  Max17048StatusProvider(Max17048& gauge, TwoWire& wire = Wire);

  const char* name() const override { return "max17048"; }
  bool begin() override;
  DeviceStatusCode refresh(DeviceStatusReport& report) override;
  const char* details() const override { return _details; }

  void setWarnPercent(float warn) { _warnPercent = warn; }
  void setErrorPercent(float error) { _errorPercent = error; }

 private:
  void setDetails(const char* msg);

  Max17048& _gauge;
  TwoWire& _wire;
  float _warnPercent = 20.0f;
  float _errorPercent = 10.0f;
  char _details[80];
  bool _present = false;
};

}  // namespace device_status
