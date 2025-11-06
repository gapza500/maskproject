#include "../../include/device_status/providers/OledStatusProvider.h"

#include <stdarg.h>
#include <algorithm>

namespace device_status {

OledStatusProvider::OledStatusProvider(screen_manager::ScreenManager& manager)
    : _manager(manager) {
  setDetails("not initialised");
}

bool OledStatusProvider::begin() {
  _initialised = true;
  _manager.setDataAreaEnabled(true);
  if (_heartbeat) {
    _manager.setData(0, 0, String(_heartbeat));
    _manager.setDataAlignment(0, 0, screen_manager::TextAlign::Center);
  }
  setDetails("screen manager ready");
  return true;
}

DeviceStatusCode OledStatusProvider::refresh(DeviceStatusReport& report) {
  report.oledData.initialised = _initialised;

  if (!_initialised) {
    report.oled = STATUS_INIT_FAIL;
    setDetails("screen inactive");
    return report.oled;
  }

  report.oledData.activeRows = 0;
  if (_heartbeat) {
    _manager.setData(0, 0, String(_heartbeat));
    report.oledData.activeRows = 1;
  }
  if (_statusLine1.length()) {
    _manager.setData(1, 0, _statusLine1);
    report.oledData.activeRows = std::max<uint8_t>(report.oledData.activeRows, 2);
  }
  if (_statusLine2.length()) {
    _manager.setData(2, 0, _statusLine2);
    report.oledData.activeRows = std::max<uint8_t>(report.oledData.activeRows, 3);
  }

  report.oledData.lastCommandOk = true;
  report.oled = STATUS_OK;
  setDetails("screen manager ok");
  return report.oled;
}

void OledStatusProvider::setStatusText(const String& line1, const String& line2) {
  _statusLine1 = line1;
  _statusLine2 = line2;
  if (_initialised) {
    if (_statusLine1.length()) {
      _manager.setData(1, 0, _statusLine1);
    }
    if (_statusLine2.length()) {
      _manager.setData(2, 0, _statusLine2);
    }
    _manager.markAllDirty();
  }
}

void OledStatusProvider::setDetails(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(_details, sizeof(_details), fmt, args);
  va_end(args);
}

}  // namespace device_status
