#pragma once

#include "DeviceStatusCodes.h"
#include "Report.h"

namespace device_status {

class StatusProvider {
 public:
  virtual ~StatusProvider() = default;

  virtual const char* name() const = 0;
  virtual bool begin() = 0;
  virtual DeviceStatusCode refresh(DeviceStatusReport& report) = 0;
  virtual const char* details() const = 0;
};

}  // namespace device_status
