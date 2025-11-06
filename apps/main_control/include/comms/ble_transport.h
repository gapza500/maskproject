#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "../../../labs/devicestatus_lib/include/device_status/Report.h"

namespace comms {
namespace ble {

struct Config {
  const char* deviceName = "StatusBridge";
};

class Transport {
 public:
  explicit Transport(const Config& cfg) : _cfg(cfg) {}

  void begin() {
    if (_started) return;
    NimBLEDevice::init(_cfg.deviceName);
    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService("180A");  // Device Information (placeholder)
    _statusChar = service->createCharacteristic("2A57", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    service->start();
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->start();
    _started = true;
  }

  void update(const device_status::DeviceStatusReport& report) {
    if (!_started || !_statusChar) return;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "SEN:%s FL:%s",
             device_status::statusToString(report.sen66),
             device_status::statusToString(report.flash));
    _statusChar->setValue(buffer);
    _statusChar->notify();
  }

 private:
  Config _cfg;
  bool _started = false;
  NimBLECharacteristic* _statusChar = nullptr;
};

}  // namespace ble
}  // namespace comms

