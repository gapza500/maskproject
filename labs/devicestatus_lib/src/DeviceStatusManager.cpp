#include "../include/device_status/DeviceStatusManager.h"

namespace device_status {

DeviceStatusManager::DeviceStatusManager(DeviceStatusReport& report) : _report(report) {}

bool DeviceStatusManager::addProvider(StatusProvider& provider) {
  if (_count >= kMaxProviders) return false;
  _providers[_count++] = &provider;
  return true;
}

bool DeviceStatusManager::beginAll() {
  bool ok = true;
  for (size_t i = 0; i < _count; ++i) {
    if (!_providers[i]->begin()) {
      ok = false;
    }
  }
  return ok;
}

void DeviceStatusManager::refreshAll() {
  for (size_t i = 0; i < _count; ++i) {
    _providers[i]->refresh(_report);
  }
}

}  // namespace device_status
