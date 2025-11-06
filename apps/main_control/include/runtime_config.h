#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace runtime_config {

struct RuntimeConfig {
  uint32_t activeDurationMs = 5UL * 60UL * 1000UL;        // time spent awake per cycle
  uint32_t sleepDurationMs = 12UL * 60UL * 1000UL;        // deep sleep duration
  uint32_t refreshIntervalMs = 1000UL;                    // dashboard/status refresh cadence (unused legacy)
  uint32_t summaryIntervalMs = 15UL * 1000UL;             // serial summary cadence
  uint32_t provisioningDisplayIntervalMs = 1000UL;        // captive portal status update
  uint32_t measurementIntervalMs = 5UL * 1000UL;          // SEN66 sampling cadence
  uint32_t syncWindowMs = 30UL * 1000UL;                  // time budget for sync stage
  uint32_t syncRetryIntervalMs = 5000UL;                  // delay between NTP attempts
  uint32_t graphScreenMs = 30UL * 1000UL;                 // particulate (graph) dwell time
  uint32_t feelingScreenMs = 10UL * 1000UL;               // comfort (feeling) dwell time
  struct Provisioning {
    const char* apSsid = "Device-Setup";
    const char* apPassword = "password123";
    IPAddress localIP = IPAddress(192, 168, 4, 1);
    IPAddress gateway = IPAddress(192, 168, 4, 1);
    IPAddress subnet = IPAddress(255, 255, 255, 0);
    int ledPin = -1;
    uint32_t connectTimeoutMs = 20000;
    uint32_t staHeartbeatIntervalMs = 5000;
    uint32_t apHeartbeatIntervalMs = 500;
    uint32_t reconnectBackoffMs = 5000;
    uint32_t portalShutdownDelayMs = 10000;
    uint16_t broadcastPort = 4210;
    bool apStaDuringProvisioning = false;
    uint32_t scanTimeoutMs = 300;
  } provisioning{};
};

inline constexpr RuntimeConfig make() {
  return RuntimeConfig{};
}

}  // namespace runtime_config
