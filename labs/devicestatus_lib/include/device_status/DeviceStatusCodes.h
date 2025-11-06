#pragma once

#include <Arduino.h>

namespace device_status {

// ----- Unified status codes for all sub-devices -----
enum DeviceStatusCode : uint8_t {
  STATUS_OK = 0,
  STATUS_WARN = 1,
  STATUS_ERROR = 2,
  STATUS_NOT_FOUND = 3,
  STATUS_INIT_FAIL = 4,
};

// Human-readable labels for logs / UI
static inline const __FlashStringHelper* statusToString(DeviceStatusCode code) {
  switch (code) {
    case STATUS_OK: return F("OK");
    case STATUS_WARN: return F("WARN");
    case STATUS_ERROR: return F("ERROR");
    case STATUS_NOT_FOUND: return F("NOT FOUND");
    case STATUS_INIT_FAIL: return F("INIT FAIL");
    default: return F("UNKNOWN");
  }
}

}  // namespace device_status

