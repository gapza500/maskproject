#include "../../include/device_status/providers/FlashStatusProvider.h"

#include <stdarg.h>
#include <string.h>

namespace device_status {

FlashStatusProvider::FlashStatusProvider(FlashStore& flash) : _flash(flash) {
  setDetails("not initialised");
}

bool FlashStatusProvider::begin() {
  _initialised = _flash.begin();
  if (!_initialised) {
    setDetails("flash init failed");
    return false;
  }
  setDetails("flash ready");
  return true;
}

DeviceStatusCode FlashStatusProvider::refresh(DeviceStatusReport& report) {
  report.flashData.initialised = _initialised;

  if (!_initialised) {
    report.flash = STATUS_INIT_FAIL;
    setDetails("flash offline");
    return report.flash;
  }

  bool ok = true;
  if (_selfTest) {
    const uint8_t pattern[] = {0xAA, 0x55, 0xFF, 0x00};
    ok = _flash.writeRecord(pattern, sizeof(pattern));
    uint8_t readback[sizeof(pattern)];
    size_t read = _flash.readLatest(readback, sizeof(readback));
    ok = ok && (read == sizeof(pattern)) && (memcmp(pattern, readback, sizeof(pattern)) == 0);
  }

  report.flashData.selfTestPassed = ok;
  report.flashData.lastError = ok ? 0 : 1;

  FlashStore::FlashStats stats = _flash.getStats();
  report.flashData.totalMB = stats.totalMB;
  report.flashData.usedMB = stats.usedMB;
  report.flashData.freeMB = stats.freeMB;
  report.flashData.usedPercent = stats.usedPercent;
  report.flashData.healthPercent = stats.healthPercent;
  report.flashData.estimatedDaysLeft = stats.estimatedDaysLeft;

  if (!ok) {
    report.flash = STATUS_WARN;
    setDetails("flash self-test failed");
  } else {
    report.flash = STATUS_OK;
    if (_selfTest) {
      setDetails("flash ok (health %.1f%%)", stats.healthPercent);
    } else {
      setDetails("flash ok (health %.1f%%)", stats.healthPercent);
    }
  }
  return report.flash;
}

void FlashStatusProvider::setDetails(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(_details, sizeof(_details), fmt, args);
  va_end(args);
}

}  // namespace device_status
