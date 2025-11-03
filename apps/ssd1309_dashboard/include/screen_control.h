#pragma once

#include <Arduino.h>

#include "dashboard_view.h"
#include "../../../modules/screen_manager/include/screen_config.h"
#include "../../../modules/screen_manager/include/screen_manager.h"
#include "../../../ds3231/include/Ds3231Clock.h"
#include "../../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.h"

namespace screen_control {

constexpr uint32_t kBaseEpoch = 1700000000UL;

enum class ScreenId : uint8_t {
  Dashboard = 0,
  Particulate,
  Gases,
  IndoorComfort,
  DeviceStatus,
  UserInfo,
  Count
};

enum class ActiveAlert : uint8_t {
  None = 0,
  Manual,
  Battery,
  Device,
  Connectivity,
  AirQuality
};

struct AirData {
  float pm1 = 8.0f;
  float pm25 = 12.5f;
  float pm4 = 16.0f;
  float pm10 = 22.0f;
  float voc = 0.32f;
  float nox = 0.018f;
  float humidity = 58.0f;
  float temperature = 27.0f;
  float co2 = 420.0f;
};

struct DeviceVitals {
  bool rtcOk = true;
  bool sen66Ok = true;
  bool flashOk = true;
  const char* batteryHealth = "GOOD";
};

struct UserMeta {
  String userId = "USR-318A";
  String ssid = "MaskNet";
  String model = "MaskProject v1";
  uint32_t lastConnectionEpoch = 0;
};

struct ControlContext {
  screen_manager::ScreenManager* screen = nullptr;
  dashboard_view::DashboardView* dashboard = nullptr;
  Ds3231Clock* rtc = nullptr;
  FlashStats* flashStats = nullptr;
  FactoryInfo* factoryInfo = nullptr;

  ScreenId activeScreen = ScreenId::Dashboard;
  ActiveAlert lastAlert = ActiveAlert::None;

  AirData air;
  DeviceVitals device;
  UserMeta user;

  uint8_t batteryPercent = 76;
  bool wifiOn = true;
  bool btOn = true;
  uint8_t linkBars = 3;
  bool manualWarn = false;
  bool alertShown = false;
  uint32_t alertShownAt = 0;
  uint32_t lastDashboardUpdateMs = 0;
  bool dataDirty = true;
};

void initialize(ControlContext& ctx,
                screen_manager::ScreenManager& screen,
                dashboard_view::DashboardView& dashboard,
                Ds3231Clock& rtc,
                FlashStats& stats,
                FactoryInfo& factory,
                ScreenId initialScreen,
                uint32_t nowMs);

void tick(ControlContext& ctx, uint32_t nowMs);
void updateDashboard(ControlContext& ctx, uint32_t nowMs, bool force = false);
void setActiveScreen(ControlContext& ctx, ScreenId id, uint32_t nowMs);
void markDataDirty(ControlContext& ctx);

void setWifi(ControlContext& ctx, bool enabled);
void setBluetooth(ControlContext& ctx, bool enabled);
void setLinkBars(ControlContext& ctx, uint8_t bars);
void setBatteryPercent(ControlContext& ctx, uint8_t percent, uint32_t nowMs);
void setPm25(ControlContext& ctx, float value, uint32_t nowMs);
void setTemperature(ControlContext& ctx, float value, uint32_t nowMs);
void setHumidity(ControlContext& ctx, float value);
void setCo2(ControlContext& ctx, float value);
void setVoc(ControlContext& ctx, float value);
void setNox(ControlContext& ctx, float value);
void setWarning(ControlContext& ctx, bool enabled, uint32_t nowMs);
void recordConnection(ControlContext& ctx, uint32_t nowMs);
void setSsid(ControlContext& ctx, const String& ssid);
void setUser(ControlContext& ctx, const String& userId);
void setModel(ControlContext& ctx, const String& model);

String formatFloat(float value, uint8_t decimals = 1);
String formatDuration(uint32_t seconds);
void formatDateTime(uint32_t epoch, char* out, size_t len);

ScreenId nextScreen(ScreenId id);
ScreenId prevScreen(ScreenId id);
const char* screenName(ScreenId id);

}  // namespace screen_control
