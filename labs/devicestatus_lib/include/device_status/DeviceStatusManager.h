#pragma once

#include <Arduino.h>

#include "Report.h"
#include "StatusProvider.h"

namespace device_status {

class DeviceStatusManager {
 public:
  explicit DeviceStatusManager(DeviceStatusReport& report);

  bool addProvider(StatusProvider& provider);
  bool beginAll();
  void refreshAll();

  size_t providerCount() const { return _count; }

 private:
  static const size_t kMaxProviders = 8;
  StatusProvider* _providers[kMaxProviders];
  size_t _count = 0;
  DeviceStatusReport& _report;
};

}  // namespace device_status
