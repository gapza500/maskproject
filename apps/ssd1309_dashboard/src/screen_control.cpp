#include "../include/screen_control.h"

#include <algorithm>

namespace screen_control {
namespace {

void ensureManagers(ControlContext& ctx) {
  if (!ctx.screen || !ctx.dashboard) {
    while (true) {
      // Fail loudly in debug builds if initialise() was skipped.
    }
  }
}

void recomputeParticulate(ControlContext& ctx) {
  ctx.air.pm1 = ctx.air.pm25 * 0.62f;
  ctx.air.pm4 = ctx.air.pm25 * 1.18f;
  ctx.air.pm10 = ctx.air.pm25 * 1.42f;
}

void updateBatteryHealth(ControlContext& ctx) {
  if (ctx.batteryPercent > 60) {
    ctx.device.batteryHealth = "GOOD";
  } else if (ctx.batteryPercent > 30) {
    ctx.device.batteryHealth = "FAIR";
  } else {
    ctx.device.batteryHealth = "LOW";
  }
}

void refreshFlashStats(ControlContext& ctx) {
  if (!ctx.flashStats || !ctx.factoryInfo) {
    return;
  }
  auto& stats = *ctx.flashStats;
  auto& factory = *ctx.factoryInfo;

  stats.usedMB = constrain(stats.usedMB, 0.0f, stats.totalMB);
  stats.freeMB = max(0.0f, stats.totalMB - stats.usedMB);
  stats.usedPercent = (stats.totalMB > 0.0f)
                          ? (stats.usedMB / stats.totalMB) * 100.0f
                          : 0.0f;
  float avgCycles = factory.totalEraseOps /
                    max(1.0f, static_cast<float>(MAX_SECTORS - 1));
  float health = 1.0f - (avgCycles / 100000.0f);
  if (health < 0.0f) health = 0.0f;
  stats.healthPercent = health * 100.0f;
  float freeBytes = stats.freeMB * 1024.0f * 1024.0f;
  float avgBytesPerDay = 3600.0f;
  float days = (avgBytesPerDay > 0.0f) ? (freeBytes / avgBytesPerDay) : 0.0f;
  if (days < 0.0f) days = 0.0f;
  if (days > 65535.0f) days = 65535.0f;
  stats.estimatedDaysLeft = static_cast<uint16_t>(days);

  ctx.device.flashOk = stats.healthPercent > 10.0f;
}

bool isLeap(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

ActiveAlert evaluateAlert(const ControlContext& ctx, uint32_t nowMs) {
  (void)nowMs;
  if (ctx.manualWarn) {
    return ActiveAlert::Manual;
  }
  if (ctx.batteryPercent < 5) {
    return ActiveAlert::Battery;
  }
  if (!ctx.device.rtcOk || !ctx.device.sen66Ok || !ctx.device.flashOk) {
    return ActiveAlert::Device;
  }
  uint32_t nowEpoch = ctx.rtc ? ctx.rtc->unixTime() : 0;
  if (ctx.user.lastConnectionEpoch == 0 ||
      (nowEpoch > ctx.user.lastConnectionEpoch &&
       nowEpoch - ctx.user.lastConnectionEpoch > 3600)) {
    return ActiveAlert::Connectivity;
  }
  if (ctx.air.pm25 > 35.0f || ctx.air.pm10 > 80.0f) {
    return ActiveAlert::AirQuality;
  }
  return ActiveAlert::None;
}

void markAllDirty(ControlContext& ctx) {
  ctx.dataDirty = true;
  if (ctx.screen) {
    ctx.screen->markAllDirty();
  }
}

}  // namespace

String formatFloat(float value, uint8_t decimals) {
  char buf[16];
  dtostrf(value, 0, decimals, buf);
  return String(buf);
}

String formatDuration(uint32_t seconds) {
  if (seconds < 60) {
    return String(seconds) + "s";
  }
  uint32_t minutes = seconds / 60;
  if (minutes < 60) {
    return String(minutes) + "m";
  }
  uint32_t hours = minutes / 60;
  if (hours < 24) {
    return String(hours) + "h" + String(minutes % 60) + "m";
  }
  uint32_t days = hours / 24;
  hours %= 24;
  return String(days) + "d" + String(hours) + "h";
}

void formatDateTime(uint32_t epoch, char* out, size_t len) {
  uint32_t secs = epoch % 60;
  uint32_t mins = (epoch / 60) % 60;
  uint32_t hours = (epoch / 3600) % 24;
  uint32_t days = epoch / 86400;
  int year = 1970;
  while (true) {
    uint32_t diy = isLeap(year) ? 366 : 365;
    if (days >= diy) {
      days -= diy;
      ++year;
    } else {
      break;
    }
  }
  static const uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int month = 0;
  while (month < 12) {
    uint8_t dim = monthDays[month];
    if (month == 1 && isLeap(year)) dim = 29;
    if (days >= dim) {
      days -= dim;
      ++month;
    } else {
      break;
    }
  }
  int day = days + 1;
  snprintf(out, len, "%04d-%02d-%02d %02u:%02u",
           year, month + 1, day,
           static_cast<unsigned>(hours),
           static_cast<unsigned>(mins));
}

ScreenId nextScreen(ScreenId id) {
  return static_cast<ScreenId>((static_cast<uint8_t>(id) + 1) %
                               static_cast<uint8_t>(ScreenId::Count));
}

ScreenId prevScreen(ScreenId id) {
  uint8_t value = static_cast<uint8_t>(id);
  if (value == 0) {
    value = static_cast<uint8_t>(ScreenId::Count) - 1;
  } else {
    --value;
  }
  return static_cast<ScreenId>(value);
}

const char* screenName(ScreenId id) {
  switch (id) {
    case ScreenId::Dashboard: return "Dashboard";
    case ScreenId::Particulate: return "Particles";
    case ScreenId::Gases: return "Gases";
    case ScreenId::IndoorComfort: return "Comfort";
    case ScreenId::DeviceStatus: return "Devices";
    case ScreenId::UserInfo: return "User";
    default: return "Unknown";
  }
}

void markDataDirty(ControlContext& ctx) {
  markAllDirty(ctx);
}

void initialize(ControlContext& ctx,
                screen_manager::ScreenManager& screen,
                dashboard_view::DashboardView& dashboard,
                Ds3231Clock& rtc,
                FlashStats& stats,
                FactoryInfo& factory,
                ScreenId initialScreen,
                uint32_t nowMs) {
  ctx.screen = &screen;
  ctx.dashboard = &dashboard;
  ctx.rtc = &rtc;
  ctx.flashStats = &stats;
  ctx.factoryInfo = &factory;
  ctx.activeScreen = initialScreen;
  ctx.lastAlert = ActiveAlert::None;
  ctx.wifiOn = true;
  ctx.btOn = true;
  ctx.linkBars = 3;
  ctx.manualWarn = false;
  ctx.alertShown = false;
  ctx.alertShownAt = 0;
  ctx.dataDirty = true;
  ctx.user.lastConnectionEpoch = rtc.unixTime();
  ctx.device.sen66Ok = true;

  recomputeParticulate(ctx);
  updateBatteryHealth(ctx);
  refreshFlashStats(ctx);
  updateDashboard(ctx, nowMs, true);
  setActiveScreen(ctx, initialScreen, nowMs);
  markAllDirty(ctx);
}

void renderActiveScreen(ControlContext& ctx);
void applyStatus(ControlContext& ctx, uint32_t nowMs);

void tick(ControlContext& ctx, uint32_t nowMs) {
  ensureManagers(ctx);
  ctx.dashboard->tick(nowMs);
  applyStatus(ctx, nowMs);
  if (ctx.dataDirty) {
    renderActiveScreen(ctx);
    ctx.screen->markAllDirty();
    ctx.dataDirty = false;
  }
  if (ctx.screen->tick(nowMs)) {
    ctx.screen->render(nowMs);
  }
}

void updateDashboard(ControlContext& ctx, uint32_t nowMs, bool force) {
  ensureManagers(ctx);
  if (!ctx.rtc) {
    return;
  }
  if (!force && (nowMs - ctx.lastDashboardUpdateMs) < 1000) {
    return;
  }
  uint32_t epochSeconds = kBaseEpoch + nowMs / 1000UL;
  ctx.rtc->setUnixTime(epochSeconds);
  ctx.device.rtcOk = ctx.rtc->running();

  dashboard_view::DashboardData data;
  data.pm25 = ctx.air.pm25;
  data.temperatureC = ctx.air.temperature;
  data.batteryPercent = ctx.batteryPercent;
  data.epochSeconds = epochSeconds;
  ctx.dashboard->updateData(data, nowMs);
  ctx.lastDashboardUpdateMs = nowMs;
}

void setActiveScreen(ControlContext& ctx, ScreenId id, uint32_t nowMs) {
  ensureManagers(ctx);
  ctx.activeScreen = id;
  bool dashboard = (id == ScreenId::Dashboard);
  ctx.screen->setDataAreaEnabled(!dashboard);
  ctx.dashboard->setActive(dashboard, nowMs);
  markAllDirty(ctx);
}

void setWifi(ControlContext& ctx, bool enabled) {
  ctx.wifiOn = enabled;
}

void setBluetooth(ControlContext& ctx, bool enabled) {
  ctx.btOn = enabled;
}

void setLinkBars(ControlContext& ctx, uint8_t bars) {
  ctx.linkBars = (bars > 3) ? 3 : bars;
}

void setBatteryPercent(ControlContext& ctx, uint8_t percent, uint32_t nowMs) {
  if (percent > 100) percent = 100;
  ctx.batteryPercent = percent;
  updateBatteryHealth(ctx);
  updateDashboard(ctx, nowMs, true);
}

void setPm25(ControlContext& ctx, float value, uint32_t nowMs) {
  if (value < 0.0f) value = 0.0f;
  ctx.air.pm25 = value;
  recomputeParticulate(ctx);
  updateDashboard(ctx, nowMs, true);
  markAllDirty(ctx);
}

void setTemperature(ControlContext& ctx, float value, uint32_t nowMs) {
  ctx.air.temperature = value;
  updateDashboard(ctx, nowMs, true);
  markAllDirty(ctx);
}

void setHumidity(ControlContext& ctx, float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 100.0f) value = 100.0f;
  ctx.air.humidity = value;
  markAllDirty(ctx);
}

void setCo2(ControlContext& ctx, float value) {
  if (value < 0.0f) value = 0.0f;
  ctx.air.co2 = value;
  markAllDirty(ctx);
}

void setVoc(ControlContext& ctx, float value) {
  if (value < 0.0f) value = 0.0f;
  ctx.air.voc = value;
  markAllDirty(ctx);
}

void setNox(ControlContext& ctx, float value) {
  if (value < 0.0f) value = 0.0f;
  ctx.air.nox = value;
  markAllDirty(ctx);
}

void setWarning(ControlContext& ctx, bool enabled, uint32_t nowMs) {
  ctx.manualWarn = enabled;
  if (!enabled) {
    if (ctx.screen) {
      ctx.screen->clearAlert();
      ctx.screen->markAllDirty();
    }
    ctx.alertShown = false;
    ctx.alertShownAt = 0;
    ctx.lastAlert = ActiveAlert::None;
  } else {
    ctx.lastAlert = ActiveAlert::None;
    ctx.alertShown = false;
    ctx.alertShownAt = 0;
    applyStatus(ctx, nowMs);
  }
}

void recordConnection(ControlContext& ctx, uint32_t nowMs) {
  if (ctx.rtc) {
    ctx.user.lastConnectionEpoch = ctx.rtc->unixTime();
  } else {
    ctx.user.lastConnectionEpoch = kBaseEpoch + nowMs / 1000UL;
  }
  updateDashboard(ctx, nowMs, true);
  markAllDirty(ctx);
}

void setSsid(ControlContext& ctx, const String& ssid) {
  ctx.user.ssid = ssid;
  markAllDirty(ctx);
}

void setUser(ControlContext& ctx, const String& userId) {
  ctx.user.userId = userId;
  markAllDirty(ctx);
}

void setModel(ControlContext& ctx, const String& model) {
  ctx.user.model = model;
  markAllDirty(ctx);
}

void renderActiveScreen(ControlContext& ctx) {
  ensureManagers(ctx);
  auto* screen = ctx.screen;
  screen->clearData();

  auto setRow = [&](uint8_t row, const __FlashStringHelper* label, const String& value) {
    if (label) {
      screen->setData(row, 0, String(label) + value);
    } else {
      screen->setData(row, 0, value);
    }
  };

  switch (ctx.activeScreen) {
    case ScreenId::Dashboard: {
      // Overlay handles dashboard drawing
      break;
    }
    case ScreenId::Particulate: {
      setRow(0, F("PM1.0 : "), formatFloat(ctx.air.pm1, 1) + F(" ug/m3"));
      setRow(1, F("PM2.5 : "), formatFloat(ctx.air.pm25, 1) + F(" ug/m3"));
      setRow(2, F("PM4.0 : "), formatFloat(ctx.air.pm4, 1) + F(" ug/m3"));
      setRow(3, F("PM10  : "), formatFloat(ctx.air.pm10, 1) + F(" ug/m3"));
      setRow(4, F("Trend : "), ctx.air.pm25 > 25.0f ? F("Rising") : F("Stable"));
      break;
    }
    case ScreenId::Gases: {
      setRow(0, F("VOC : "), formatFloat(ctx.air.voc, 2) + F(" ppm"));
      setRow(1, F("NOx : "), formatFloat(ctx.air.nox, 3) + F(" ppm"));
      for (uint8_t row = 2; row < screen_config::kDataRows; ++row) {
        screen->setData(row, 0, String());
      }
      break;
    }
    case ScreenId::IndoorComfort: {
      setRow(0, F("Humidity : "), formatFloat(ctx.air.humidity, 1) + F(" %RH"));
      setRow(1, F("Temp     : "), formatFloat(ctx.air.temperature, 1) + F(" C"));
      setRow(2, F("CO2      : "), formatFloat(ctx.air.co2, 0) + F(" ppm"));
      setRow(3, F("Dew Pt   : "),
             formatFloat(ctx.air.temperature - (100.0f - ctx.air.humidity) / 5.0f, 1) + F(" C"));
      setRow(4, F("Comfort  : "),
             (ctx.air.humidity > 30 && ctx.air.humidity < 65) ? F("OK") : F("Adjust"));
      break;
    }
    case ScreenId::DeviceStatus: {
      float rtcTemp = ctx.rtc ? ctx.rtc->temperatureC() : 0.0f;
      setRow(0, F("RTC    : "),
             String(ctx.device.rtcOk ? F("OK ") : F("CHK ")) +
                 F("T=") + formatFloat(rtcTemp, 1) + F("C"));
      setRow(1, F("SEN66  : "), ctx.device.sen66Ok ? F("OK") : F("CHK"));
      setRow(2, F("Flash  : "),
             String(ctx.device.flashOk ? F("OK ") : F("CHK ")) +
                 String(ctx.factoryInfo ? ctx.factoryInfo->flashModel : ""));
      setRow(3, F("Usage  : "),
             formatFloat(ctx.flashStats ? ctx.flashStats->usedMB : 0, 2) + F("/") +
                 formatFloat(ctx.flashStats ? ctx.flashStats->totalMB : 0, 1) + F("MB"));
      setRow(4, F("Health : "),
             formatFloat(ctx.flashStats ? ctx.flashStats->healthPercent : 0, 0) +
                 F("% | Days ") +
                 String(ctx.flashStats ? ctx.flashStats->estimatedDaysLeft : 0));
      break;
    }
    case ScreenId::UserInfo: {
      char ts[22];
      formatDateTime(ctx.rtc ? ctx.rtc->unixTime() : kBaseEpoch, ts, sizeof(ts));
      setRow(0, F("User : "), ctx.user.userId);
      setRow(1, F("Model: "), ctx.user.model);
      setRow(2, F("SSID : "), ctx.user.ssid);
      uint32_t nowEpoch = ctx.rtc ? ctx.rtc->unixTime() : kBaseEpoch;
      uint32_t delta = nowEpoch > ctx.user.lastConnectionEpoch
                           ? nowEpoch - ctx.user.lastConnectionEpoch
                           : 0;
      setRow(3, F("Last link : "), formatDuration(delta));
      setRow(4, F("Time : "), ts);
      break;
    }
    default: {
      break;
    }
  }

  ctx.dataDirty = false;
}

void applyStatus(ControlContext& ctx, uint32_t nowMs) {
  ensureManagers(ctx);
  auto* screen = ctx.screen;

  screen->setStatusIcon(0, ctx.wifiOn ? screen_config::IconId::WifiOn
                                      : screen_config::IconId::WifiOff);
  screen->setStatusValueVisible(0, false);

  screen->setStatusIcon(1, ctx.btOn ? screen_config::IconId::Bluetooth
                                    : screen_config::IconId::Dot);
  screen->setStatusValueVisible(1, false);

  screen_config::IconId signalIcon = screen_config::IconId::Signal0;
  switch (ctx.linkBars) {
    case 1: signalIcon = screen_config::IconId::Signal1; break;
    case 2: signalIcon = screen_config::IconId::Signal2; break;
    case 3: signalIcon = screen_config::IconId::Signal3; break;
    default: signalIcon = screen_config::IconId::Signal0; break;
  }
  screen->setStatusIcon(2, signalIcon);
  screen->setStatusValueVisible(2, false);

  ActiveAlert newReason = evaluateAlert(ctx, nowMs);
  bool showWarning = (newReason != ActiveAlert::None);
  screen->setStatusIcon(3, showWarning ? screen_config::IconId::Warning
                                       : screen_config::IconId::Dot);
  screen->setStatusValueVisible(3, false);

  if (ctx.alertShown) {
    if (newReason != ctx.lastAlert) {
      screen->clearAlert();
      ctx.alertShown = false;
      ctx.alertShownAt = 0;
      screen->markAllDirty();
    } else if (nowMs - ctx.alertShownAt >= 10000) {
      screen->clearAlert();
      ctx.alertShown = false;
      ctx.alertShownAt = 0;
      screen->markAllDirty();
    }
  }

  if (!ctx.alertShown && newReason != ActiveAlert::None && newReason != ctx.lastAlert) {
    screen_manager::AlertCfg cfg;
    switch (newReason) {
      case ActiveAlert::Manual:
        cfg.level = screen_config::AlertLevel::Warn;
        cfg.title = "Warning";
        cfg.detail = "Manual alert";
        break;
      case ActiveAlert::Battery:
        cfg.level = screen_config::AlertLevel::Crit;
        cfg.title = "Battery Critical";
        cfg.detail = "Battery below 5%";
        break;
      case ActiveAlert::Device:
        cfg.level = screen_config::AlertLevel::Warn;
        cfg.title = "Device Status";
        cfg.detail = "Check sensors/flash";
        break;
      case ActiveAlert::Connectivity:
        cfg.level = screen_config::AlertLevel::Warn;
        cfg.title = "Connectivity";
        cfg.detail = "No uplink >1h";
        break;
      case ActiveAlert::AirQuality:
        cfg.level = screen_config::AlertLevel::Crit;
        cfg.title = "PM High";
        cfg.detail = "Air quality degraded";
        break;
      default:
        break;
    }
    if (cfg.title && screen->showAlert(cfg)) {
      ctx.alertShown = true;
      ctx.alertShownAt = nowMs;
      ctx.lastAlert = newReason;
      screen->markAllDirty();
    }
  }

  if (newReason == ActiveAlert::None) {
    if (ctx.alertShown) {
      screen->clearAlert();
      ctx.alertShown = false;
      ctx.alertShownAt = 0;
      screen->markAllDirty();
    }
    ctx.lastAlert = ActiveAlert::None;
  }

  screen->setStatusIcon(4, screen_config::batteryIconForPercent(ctx.batteryPercent));
  screen->setStatusValueVisible(4, false);
  screen->touch();
}

}  // namespace screen_control
