#pragma once

#include <WiFi.h>
#include <time.h>

#include "../../modules/ds3231/include/Ds3231Clock.h"
#include "../../labs/devicestatus_lib/device_status.h"

struct TimeSyncConfig {
  const char* ntpServer = "pool.ntp.org";
  long gmtOffsetSeconds = 7L * 3600L;  // UTC+7 by default
  int daylightOffsetSeconds = 0;
  uint32_t minSyncIntervalMs = 60UL * 60UL * 1000UL;  // 1 hour
};

struct TimeSyncState {
  uint32_t lastSuccessMs = 0;
};

inline bool syncRtcFromNtp(Ds3231Clock& rtc,
                           const TimeSyncConfig& cfg,
                           TimeSyncState& state,
                           uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (cfg.minSyncIntervalMs && (nowMs - state.lastSuccessMs) < cfg.minSyncIntervalMs) {
    return false;
  }

  configTime(cfg.gmtOffsetSeconds, cfg.daylightOffsetSeconds, cfg.ntpServer);

  struct tm timeinfo {};
  if (!getLocalTime(&timeinfo, 2000)) {
    return false;
  }

  time_t raw = mktime(&timeinfo);
  DateTime dt(raw);
  rtc.adjust(dt);
  device_status::GlobalTime::update(dt);
  state.lastSuccessMs = nowMs;
  return true;
}

inline void disconnectWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

