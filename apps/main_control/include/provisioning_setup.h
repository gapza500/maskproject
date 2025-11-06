#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#include "runtime_config.h"
#include "../../labs/ProvisioningManager/ProvisioningManager.h"

namespace provisioning_setup {

inline ProvisioningConfig makeConfig() {
  const auto cfgSrc = runtime_config::make();
  const auto& prov = cfgSrc.provisioning;
  ProvisioningConfig cfg{
      .apSsid = prov.apSsid,
      .apPassword = prov.apPassword,
      .localIP = prov.localIP,
      .gateway = prov.gateway,
      .subnet = prov.subnet,
      .ledPin = prov.ledPin,
      .connectTimeoutMs = prov.connectTimeoutMs,
      .staHeartbeatIntervalMs = prov.staHeartbeatIntervalMs,
      .apHeartbeatIntervalMs = prov.apHeartbeatIntervalMs,
      .reconnectBackoffMs = prov.reconnectBackoffMs,
      .portalShutdownDelayMs = prov.portalShutdownDelayMs,
      .broadcastPort = prov.broadcastPort,
      .apStaDuringProvisioning = prov.apStaDuringProvisioning,
      .scanTimeoutMs = prov.scanTimeoutMs,
  };
  return cfg;
}

}  // namespace provisioning_setup
