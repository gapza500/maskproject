#pragma once

#include "../StatusProvider.h"
#include "../../../../../modules/screen_manager/include/screen_manager.h"
#include "../../../../../modules/screen_manager/include/screen_config.h"

namespace device_status {

class OledStatusProvider : public StatusProvider {
 public:
  explicit OledStatusProvider(screen_manager::ScreenManager& manager);

  const char* name() const override { return "oled"; }
  bool begin() override;
  DeviceStatusCode refresh(DeviceStatusReport& report) override;
  const char* details() const override { return _details; }

  void setHeartbeatMessage(const char* msg) { _heartbeat = msg; }
  void setStatusText(const String& line1, const String& line2 = String());

 private:
  void setDetails(const char* fmt, ...);

  screen_manager::ScreenManager& _manager;
  const char* _heartbeat = nullptr;
  String _statusLine1;
  String _statusLine2;
  char _details[80];
  bool _initialised = false;
};

}  // namespace device_status
