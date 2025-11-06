#pragma once

#include "../StatusProvider.h"
#include "../../../../../modules/flash/include/FlashStore.h"

namespace device_status {

class FlashStatusProvider : public StatusProvider {
 public:
  explicit FlashStatusProvider(FlashStore& flash);

  const char* name() const override { return "w25q128"; }
  bool begin() override;
  DeviceStatusCode refresh(DeviceStatusReport& report) override;
  const char* details() const override { return _details; }

  void enableSelfTest(bool enable) { _selfTest = enable; }

 private:
  void setDetails(const char* fmt, ...);

  FlashStore& _flash;
  char _details[96];
  bool _initialised = false;
  bool _selfTest = false;
};

}  // namespace device_status
