#pragma once

#include "../StatusProvider.h"
#include "../../../../../modules/ds3231/include/Ds3231Clock.h"

namespace device_status {

class Ds3231StatusProvider : public StatusProvider {
 public:
  explicit Ds3231StatusProvider(Ds3231Clock& rtc);

  const char* name() const override { return "ds3231"; }
  bool begin() override;
  DeviceStatusCode refresh(DeviceStatusReport& report) override;
  const char* details() const override { return _details; }

 private:
  void setDetails(const char* fmt, ...);

  Ds3231Clock& _rtc;
  char _details[96];
  bool _present = false;
};

}  // namespace device_status
