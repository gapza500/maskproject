#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SensirionI2cSen66.h>
#include <time.h>
#include <Preferences.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <algorithm>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/gpio.h>
#endif

#define SCREEN_BACKEND_U8G2
#include <U8g2lib.h>

#include "include/time_sync.h"
#include "include/comms.h"
#include "include/runtime_config.h"
#include "include/logging.h"

#include "../ssd1309_dashboard/include/dashboard_view.h"
#include "../ssd1309_dashboard/include/screen_control.h"
#include "../ssd1309_dashboard/include/screen_serial.h"

#include "../../modules/esp32_devkit_v1/pins_esp32_devkit_v1.h"
#include "../../modules/flash/include/FlashStore.h"
#include "../../modules/ds3231/include/Ds3231Clock.h"
#include "../../modules/max17048/Max17048.h"
#include "../../modules/screen_manager/include/screen_config.h"
#include "../../modules/screen_manager/include/screen_manager.h"
#include "../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.h"
#include "../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.cpp"
#include "../../labs/FlashDatabase/miniFlashDataBase_v2_0/UploadHelpers.cpp"
#include "../../labs/devicestatus_lib/device_status.h"
#include "../../labs/devicestatus_lib/include/device_status/providers/Sen66StatusProvider.h"
#include "../../labs/devicestatus_lib/include/device_status/providers/Ds3231StatusProvider.h"
#include "../../labs/devicestatus_lib/include/device_status/providers/FlashStatusProvider.h"
#include "../../labs/devicestatus_lib/src/DeviceStatusManager.cpp"
#include "../../labs/devicestatus_lib/src/GlobalTime.cpp"
#include "../../labs/devicestatus_lib/src/providers/Sen66StatusProvider.cpp"
#include "../../labs/devicestatus_lib/src/providers/Ds3231StatusProvider.cpp"
#include "../../labs/devicestatus_lib/src/providers/FlashStatusProvider.cpp"

#include "../../modules/ds3231/src/Ds3231Clock.cpp"
#include "../../modules/flash/src/FlashStore.cpp"
#include "../../modules/screen_manager/src/screen_manager.cpp"

#include "../ssd1309_dashboard/src/dashboard_view.cpp"
#include "../ssd1309_dashboard/src/screen_control.cpp"
#include "../ssd1309_dashboard/src/screen_serial.cpp"

using namespace board_pins::esp32_devkit_v1;

namespace {
constexpr uint32_t kI2cBusSpeedHz = 100000UL;  // SEN66 only supports 100 kHz

const runtime_config::RuntimeConfig runtimeCfg = runtime_config::make();

SensirionI2cSen66 sen66Driver;
Ds3231Clock rtc;
FlashStore flash;
Max17048 batteryGauge;
FlashLogger* flashLogger = nullptr;
FlashLoggerConfig flashLoggerCfg;
bool flashLoggerReady = false;
screen_manager::ScreenManager screenManager;
dashboard_view::DashboardView dashboardView;
screen_control::ControlContext screenCtx;
FlashStats screenFlashStats{};
FactoryInfo screenFactoryInfo{};
String lastKnownNetwork = "Offline";
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oledDisplay(U8G2_R0,
                                                 U8X8_PIN_NONE,
                                                 I2C_SCL_PIN,
                                                 I2C_SDA_PIN);

bool lastWifiConnected = false;
constexpr bool kEnableLocalApi = false;
constexpr bool kEnableBle = false;

enum class AutoScreenState : uint8_t { Graph, Feeling };
AutoScreenState autoScreenState = AutoScreenState::Graph;
uint32_t autoScreenLastSwitchMs = 0;
bool autoScreenEnabled = true;

comms::cloud::Config cloudConfig{
    .url = nullptr,                   // set to your endpoint (e.g. "http://example.com/api/logs")
    .authHeader = nullptr,
    .authToken = nullptr,
    .publishIntervalMs = 60'000,
    .batchSize = 64,
    .policy = {},
    .prefsNamespace = "cloud",
    .enabled = false};
comms::cloud::State cloudState{};

comms::local_api::Config localApiConfig{};
comms::local_api::Service localApi(localApiConfig);

comms::ble::Config bleConfig{};
comms::ble::Transport bleTransport(bleConfig);

const int kButtonPin = 15;
const uint32_t kButtonDebounceMs = 50;
const uint32_t kButtonLongPressMs = 5000;
int buttonLastReading = LOW;
int buttonStableState = LOW;
uint32_t buttonLastDebounceMs = 0;
uint32_t buttonPressStartMs = 0;
bool buttonLongPressHandled = false;

device_status::DeviceStatusReport report;
device_status::DeviceStatusManager manager(report);

device_status::Sen66StatusProvider sen66Provider(sen66Driver, Wire);
device_status::Ds3231StatusProvider rtcProvider(rtc);
device_status::FlashStatusProvider flashProvider(flash);
TimeSyncConfig timeSyncConfig{};
TimeSyncState timeSyncState{};

uint32_t stateStartMs = 0;
uint32_t lastRefreshMs = 0;
uint32_t lastSummaryMs = 0;
uint32_t lastMeasurementMs = 0;
uint32_t lastMeasurementAttemptMs = 0;
uint32_t lastSyncAttemptMs = 0;
uint32_t measureWindowEndUnix = 0;
bool measureWindowEndValid = false;
uint32_t measureWindowEndMs = 0;
bool measureWindowEndMsValid = false;
uint32_t lastStateLogMs = 0;

enum class RunState : uint8_t { Init, Measure, Sync, Sleep };
RunState state = RunState::Init;

const char* stateName(RunState s) {
  switch (s) {
    case RunState::Init: return "Init";
    case RunState::Measure: return "Measure";
    case RunState::Sync: return "Sync";
    case RunState::Sleep: return "Sleep";
  }
  return "Unknown";
}

bool syncComplete = false;
bool hasSuccessfulMeasurement = false;
bool measurementWindowWarned = false;
uint32_t lastMeasurementUnix = 0;

Preferences sysPrefs;
bool isProvisionedFlag = false;

void initScreen(uint32_t nowMs);
void refreshStatus(uint32_t nowMs);
void syncScreen(uint32_t nowMs);
void logSummary();
void prepareForSleep();
void enterSleep(uint32_t sleepMs);

void handleInit(uint32_t nowMs);
bool performMeasurement(uint32_t nowMs);
bool recordMeasurement(uint32_t nowMs);
bool performSync(uint32_t nowMs);
void transitionTo(RunState nextState, uint32_t nowMs);
void updateFlashStats();
void handleButton(uint32_t nowMs);
void handleShortPress(uint32_t nowMs);
void performFactoryReset(uint32_t nowMs);
void handleSerialCommand(const String& cmd, uint32_t nowMs);
void updateAutoScreens(uint32_t nowMs);
void disableAutoScreens(uint32_t nowMs);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  logx::info("main", "boot");
#if defined(ARDUINO_ARCH_ESP32)
  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  const char* cause = "unknown";
  switch (wakeCause) {
    case ESP_SLEEP_WAKEUP_TIMER: cause = "timer"; break;
    case ESP_SLEEP_WAKEUP_EXT0: cause = "button"; break;
    case ESP_SLEEP_WAKEUP_EXT1: cause = "gpio"; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: cause = "touch"; break;
    case ESP_SLEEP_WAKEUP_ULP: cause = "ulp"; break;
    default: break;
  }
  logx::infof("main", "wake cause: %s", cause);
#endif

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, kI2cBusSpeedHz);
  Wire.setClock(kI2cBusSpeedHz);
  rtc.begin(Wire);
  if (!batteryGauge.begin()) {
    logx::warn("battery", "MAX17048 not detected");
  }

  pinMode(kButtonPin, INPUT_PULLDOWN);
  buttonLastReading = digitalRead(kButtonPin);
  buttonStableState = buttonLastReading;
  buttonLastDebounceMs = millis();
  buttonLongPressHandled = false;
  buttonPressStartMs = millis();

  sysPrefs.begin("sys", false);
  isProvisionedFlag = true;  // force provisioning bypass for offline operation
  WiFi.mode(WIFI_OFF);

  initScreen(millis());

  flashProvider.enableSelfTest(false);
  flash.simulateUsage(/*usedSectors=*/0, /*totalEraseOps=*/0, /*avgBytesPerDay=*/0.0f);

  flashLogger = new FlashLogger();
  if (!flashLogger) {
    logx::error("flash", "allocation failed");
  }
  flashLoggerCfg = FlashLoggerConfig{};
  flashLoggerCfg.spi_cs_pin = VSPI_CS0_PIN;
  flashLoggerCfg.spi_clock_hz = 20'000'000;
  flashLoggerCfg.rtc = &rtc.raw();
  flashLoggerCfg.persistConfig = true;
  flashLoggerCfg.dailyBytesHint = 4096;
  if (!flashLogger || !flashLogger->begin(flashLoggerCfg)) {
    logx::error("flash", "FlashLogger init failed");
    flashLoggerReady = false;
  } else {
    flashLoggerReady = true;
    updateFlashStats();
  }

  if (flashLoggerReady) {
    comms::cloud::init(cloudConfig, cloudState, *flashLogger);
  }
  if (kEnableLocalApi) {
    localApi.setLogger(flashLogger);
    localApi.begin();
  }
  if (kEnableBle) {
    bleTransport.begin();
  }

  manager.addProvider(sen66Provider);
  manager.addProvider(rtcProvider);
  manager.addProvider(flashProvider);

  if (!manager.beginAll()) {
    logx::warn("main", "one or more providers failed init");
  }

  uint32_t now = millis();
  stateStartMs = now;
  lastRefreshMs = now;
  lastSummaryMs = now;

  transitionTo(RunState::Init, now);

  logx::info("serial", "ready. Type 'help' for commands.");
}

void loop() {
  const uint32_t now = millis();

  if ((now - lastRefreshMs) >= runtimeCfg.measurementIntervalMs) {
    refreshStatus(now);
    lastRefreshMs = now;
  }

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (!cmd.isEmpty()) {
      handleSerialCommand(cmd, now);
    }
  }

  handleButton(now);

  if (kEnableLocalApi) {
    localApi.loop(report);
  }

  switch (state) {
    case RunState::Init:
      handleInit(now);
      break;
    case RunState::Measure:
      if (performMeasurement(now)) {
        transitionTo(RunState::Sync, now);
      }
      break;
    case RunState::Sync:
      if (performSync(now)) {
        transitionTo(RunState::Sleep, now);
      }
      break;
    case RunState::Sleep:
      logSummary();
      prepareForSleep();
      enterSleep(runtimeCfg.sleepDurationMs);
      transitionTo(RunState::Measure, millis());
      break;
  }

  screen_control::updateDashboard(screenCtx, now);
  screen_control::tick(screenCtx, now);
  updateAutoScreens(now);

  if ((now - lastStateLogMs) >= 15000UL) {
    lastStateLogMs = now;
    logx::infof("state", "loop in %s (measureEndMsValid=%d msRemaining=%ld)",
                stateName(state),
                measureWindowEndMsValid ? 1 : 0,
                measureWindowEndMsValid ? static_cast<long>(measureWindowEndMs > now ? measureWindowEndMs - now : 0) : -1L);
  }
}

namespace {

void transitionTo(RunState nextState, uint32_t nowMs) {
  const char* prevName = stateName(state);
  const char* nextName = stateName(nextState);
  if (state != nextState) {
    logx::infof("state", "transition %s -> %s", prevName, nextName);
  } else {
    logx::infof("state", "re-enter %s", nextName);
  }
  state = nextState;
  stateStartMs = nowMs;
  switch (state) {
    case RunState::Init:
      syncComplete = false;
      hasSuccessfulMeasurement = false;
      measurementWindowWarned = false;
      lastRefreshMs = nowMs - runtimeCfg.measurementIntervalMs;
      autoScreenState = AutoScreenState::Graph;
      autoScreenLastSwitchMs = nowMs;
      autoScreenEnabled = true;
      measureWindowEndMsValid = false;
      measureWindowEndValid = false;
      break;
    case RunState::Measure:
      syncComplete = false;
      hasSuccessfulMeasurement = false;
      measurementWindowWarned = false;
      report.sen66Data.dataValid = false;
      lastRefreshMs = nowMs - runtimeCfg.measurementIntervalMs;
      lastMeasurementMs = nowMs - runtimeCfg.measurementIntervalMs;
      lastMeasurementAttemptMs = nowMs - runtimeCfg.measurementIntervalMs;
      lastSummaryMs = nowMs;
      autoScreenState = AutoScreenState::Graph;
      autoScreenLastSwitchMs = nowMs;
      autoScreenEnabled = true;
      measureWindowEndMs = nowMs + runtimeCfg.activeDurationMs;
      measureWindowEndMsValid = true;
      {
        DateTime rtcNow = rtc.now();
        if (rtcNow.year() >= 2020 && rtcNow.unixtime() > 0) {
          uint32_t durationSec = runtimeCfg.activeDurationMs / 1000UL;
          if (durationSec == 0) durationSec = 1;
          measureWindowEndUnix = rtcNow.unixtime() + durationSec;
          measureWindowEndValid = true;
        } else {
          measureWindowEndValid = false;
        }
      }
      break;
    case RunState::Sync:
      syncComplete = false;
      measurementWindowWarned = false;
      lastSyncAttemptMs = nowMs - runtimeCfg.syncRetryIntervalMs;
      break;
    case RunState::Sleep:
      measureWindowEndMsValid = false;
      measureWindowEndValid = false;
      break;
  }
}

void handleInit(uint32_t nowMs) {
  manager.refreshAll();
  syncScreen(nowMs);
  if ((nowMs - lastSummaryMs) >= runtimeCfg.summaryIntervalMs) {
    logSummary();
    lastSummaryMs = nowMs;
  }

  transitionTo(RunState::Measure, nowMs);
}

void handleButton(uint32_t nowMs) {
  int reading = digitalRead(kButtonPin);
  if (reading != buttonLastReading) {
    buttonLastDebounceMs = nowMs;
    buttonLastReading = reading;
  }
  if ((nowMs - buttonLastDebounceMs) < kButtonDebounceMs) return;

  if (reading != buttonStableState) {
    buttonStableState = reading;
    if (buttonStableState == HIGH) {
      buttonPressStartMs = nowMs;
      buttonLongPressHandled = false;
    } else {
      if (!buttonLongPressHandled && (nowMs - buttonPressStartMs) < kButtonLongPressMs) {
        handleShortPress(nowMs);
      }
    }
  }

  if (buttonStableState == HIGH && !buttonLongPressHandled &&
      (nowMs - buttonPressStartMs) >= kButtonLongPressMs) {
    buttonLongPressHandled = true;
    performFactoryReset(nowMs);
  }
}

void handleShortPress(uint32_t nowMs) {
  screen_control::ScreenId next = screen_control::nextScreen(screenCtx.activeScreen);
  screen_control::setActiveScreen(screenCtx, next, nowMs);
  logx::infof("button", "short press -> %s", screen_control::screenName(screenCtx.activeScreen));
  disableAutoScreens(nowMs);
}

void handleSerialCommand(const String& cmd, uint32_t nowMs) {
  String trimmed = cmd;
  trimmed.trim();
  if (trimmed.isEmpty()) {
    return;
  }

  if (trimmed.equalsIgnoreCase("help") || trimmed.equalsIgnoreCase("?")) {
    Serial.println(F("\n[serial] Available commands:"));
    Serial.println(F("  help / ?           - show this help"));
    Serial.println(F("  status             - dump device status summary"));
    Serial.println(F("  sample             - print latest sensor reading"));
    Serial.println(F("  button             - simulate a short press (cycle screen)"));
    Serial.println(F("  screen/...         - dashboard controls (see below)"));
    Serial.println(F("  alert ack [min]    - silence connectivity alert for minutes (default 60)"));
    Serial.println(F("  alert resume       - resume connectivity alerting"));
    Serial.println(F("  flash <cmd>        - miniFlash shell proxy (try 'flash help')\n"));
    screen_serial::printHelp();
    if (flashLoggerReady && flashLoggerCfg.enableShell) {
      Serial.println(F("\n[flash] Common flash commands:"));
      Serial.println(F("  flash help         - list full flash logger command set"));
      Serial.println(F("  flash ls           - list recorded days on flash"));
      Serial.println(F("  flash ls sectors   - list sectors for selected day"));
      Serial.println(F("  flash q latest 10  - view latest 10 log lines"));
      Serial.println(F("  flash export 50    - stream 50 records from cursor (auto-saves)"));
      Serial.println(F("  flash cursor show  - display current sync cursor"));
      Serial.println(F("  flash factory      - print flash diagnostics"));
      Serial.println(F("  flash reset <code> - factory reset logs (requires 12-digit code)"));
    }
    return;
  }

  if (trimmed.equalsIgnoreCase("status") || trimmed.equalsIgnoreCase("summary")) {
    refreshStatus(nowMs);
    logSummary();
    return;
  }

  if (trimmed.equalsIgnoreCase("sample")) {
    refreshStatus(nowMs);
    Serial.println(F("[serial] Latest measurement"));
    if (report.sen66Data.dataValid) {
      Serial.print(F("  PM2.5 : "));
      Serial.print(report.sen66Data.pm25, 2);
      Serial.print(F(" ug/m3  PM10: "));
      Serial.print(report.sen66Data.pm10, 2);
      Serial.print(F("  Temp: "));
      Serial.print(report.sen66Data.temperatureC, 2);
      Serial.print(F(" C  RH: "));
      Serial.print(report.sen66Data.humidity, 1);
      Serial.println(F(" %"));
      Serial.print(F("  VOC: "));
      Serial.print(report.sen66Data.vocIndex, 2);
      Serial.print(F("  NOx: "));
      Serial.print(report.sen66Data.noxIndex, 2);
      Serial.print(F("  CO2: "));
      Serial.println(report.sen66Data.co2ppm);
    } else {
      Serial.println(F("  No valid SEN66 sample yet."));
    }
    Serial.print(F("  RTC unix: "));
    Serial.println(report.rtcData.unixTime);
    Serial.print(F("  Flash used: "));
    Serial.print(report.flashData.usedPercent, 1);
    Serial.print(F("%  Health: "));
    Serial.println(report.flashData.healthPercent, 1);
    return;
  }

  if (trimmed.equalsIgnoreCase("button")) {
    handleShortPress(nowMs);
    logx::infof("serial", "Screen -> %s", screen_control::screenName(screenCtx.activeScreen));
    return;
  }

  if (trimmed.equalsIgnoreCase("sys provisioned")) {
    sysPrefs.putBool("provisioned", true);
    isProvisionedFlag = true;
    lastKnownNetwork = F("Offline");
    logx::info("sys", "provisioning flag set to true");
    Serial.println(F("[sys] Provisioning flag set. Rebooting..."));
    Serial.flush();
    delay(200);
    ESP.restart();
    return;
  }

  if (flashLoggerReady && flashLoggerCfg.enableShell) {
    String lowered = trimmed;
    lowered.toLowerCase();
    if (lowered == "flash") {
      flashLogger->handleCommand("help", Serial);
      return;
    }
    if (lowered.startsWith("flash ")) {
      String inner = trimmed.substring(6);
      inner.trim();
      if (inner.isEmpty()) inner = "help";
      if (flashLogger->handleCommand(inner, Serial)) {
        return;
      }
    }
    if (flashLogger->handleCommand(trimmed, Serial)) {
      return;
    }
  }

  String lowered = trimmed;
  lowered.toLowerCase();
  if (screen_serial::handleCommand(screenCtx, lowered, nowMs)) {
    disableAutoScreens(nowMs);
    screen_control::markDataDirty(screenCtx);
    return;
  }

  logx::warnf("serial", "Unknown command: %s", trimmed.c_str());
  logx::info("serial", "Type 'help' for a full list.");
}

void performFactoryReset(uint32_t nowMs) {
  logx::warn("button", "long press: factory reset");

  {
    Preferences pref;
    if (pref.begin("sys", false)) {
      pref.clear();
      pref.end();
    }
  }

  if (cloudConfig.prefsNamespace) {
    Preferences pref;
    if (pref.begin(cloudConfig.prefsNamespace, false)) {
      pref.clear();
      pref.end();
    }
  }

  if (flashLoggerReady) {
    const char* code = flashLoggerCfg.resetCode12 ? flashLoggerCfg.resetCode12 : "000000000000";
    if (!flashLogger->factoryReset(code)) {
      logx::error("flash", "factoryReset failed");
    }
    flashLogger->clearCursor();
    if (cloudConfig.prefsNamespace) {
      flashLogger->saveCursorNVS(cloudConfig.prefsNamespace, "cursor");
    } else {
      flashLogger->saveCursorNVS();
    }
    updateFlashStats();
  }

  isProvisionedFlag = true;
  cloudState.cursorLoaded = false;

  screenManager.setData(0, 0, String(F("Factory reset")));
  screenManager.setData(1, 0, String(F("Restarting...")));
  screen_control::markDataDirty(screenCtx);
  screen_control::tick(screenCtx, nowMs);
  delay(500);
  ESP.restart();
}

void initScreen(uint32_t nowMs) {
  oledDisplay.setI2CAddress(0x3C << 1);
  oledDisplay.begin();
  oledDisplay.setBusClock(kI2cBusSpeedHz);
  oledDisplay.setPowerSave(0);
  oledDisplay.setPowerSave(0);
  screenManager.begin(oledDisplay);
  dashboardView.begin(screenManager);
  screenManager.setDataAreaEnabled(true);
  screenManager.setDataFramesEnabled(false);
  screenManager.setStatusFramesEnabled(true);

  FlashStore::FlashStats stats = flash.getStats();
  if (!isnan(report.flashData.totalMB)) {
    stats.totalMB = report.flashData.totalMB;
    stats.usedMB = report.flashData.usedMB;
    stats.freeMB = report.flashData.freeMB;
    stats.usedPercent = report.flashData.usedPercent;
    stats.healthPercent = report.flashData.healthPercent;
    stats.estimatedDaysLeft = report.flashData.estimatedDaysLeft;
  }
  screenFlashStats.totalMB = stats.totalMB;
  screenFlashStats.usedMB = stats.usedMB;
  screenFlashStats.freeMB = stats.freeMB;
  screenFlashStats.usedPercent = stats.usedPercent;
  screenFlashStats.healthPercent = stats.healthPercent;
  screenFlashStats.estimatedDaysLeft = stats.estimatedDaysLeft;

  memset(&screenFactoryInfo, 0, sizeof(screenFactoryInfo));
  screenFactoryInfo.magic = 0x46414354UL;
  strncpy(screenFactoryInfo.model, "MainControl", sizeof(screenFactoryInfo.model) - 1);
  strncpy(screenFactoryInfo.flashModel, "W25Q128", sizeof(screenFactoryInfo.flashModel) - 1);
  strncpy(screenFactoryInfo.deviceId, "000000000000", sizeof(screenFactoryInfo.deviceId) - 1);

  screen_control::initialize(screenCtx,
                             screenManager,
                             dashboardView,
                             rtc,
                             screenFlashStats,
                             screenFactoryInfo,
                             screen_control::ScreenId::Dashboard,
                             nowMs);

  screen_control::setModel(screenCtx, String(screenFactoryInfo.model));
  screen_control::setUser(screenCtx, String(screenFactoryInfo.deviceId));
  screen_control::setWifi(screenCtx, WiFi.status() == WL_CONNECTED);
  screen_control::setLinkBars(screenCtx, WiFi.status() == WL_CONNECTED ? 3 : 0);
  screen_control::recordConnection(screenCtx, nowMs);
  screen_control::setBatteryPercent(screenCtx, 76, nowMs);
  screen_control::setBluetooth(screenCtx, true);
  screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::Dashboard, nowMs);
  screen_control::markDataDirty(screenCtx);
  screen_control::tick(screenCtx, nowMs);
}

void refreshStatus(uint32_t nowMs) {
  manager.refreshAll();
  report.oled = device_status::STATUS_OK;
  report.oledData.initialised = true;
  report.oledData.lastCommandOk = true;

  static uint32_t lastSen66ReportMs = 0;
  static bool lastSen66Valid = false;
  if (report.sen66Data.dataValid) {
    if (!lastSen66Valid) {
      logx::infof("sen66",
                  "initial reading pm25=%.2f ug/m3 temp=%.2fC humidity=%.1f%%",
                  report.sen66Data.pm25,
                  report.sen66Data.temperatureC,
                  report.sen66Data.humidity);
    }
    lastSen66Valid = true;
  } else if (nowMs - lastSen66ReportMs >= 3000) {
    logx::debugf("sen66",
                 "waiting for data status=%s detail=%s",
                 device_status::statusToString(report.sen66),
                 sen66Provider.details());
    lastSen66ReportMs = nowMs;
    lastSen66Valid = false;
  }

  syncScreen(nowMs);
  bleTransport.update(report);
}

void syncScreen(uint32_t nowMs) {
  const wl_status_t wifiStatus = WiFi.status();
  screen_control::setWifi(screenCtx, wifiStatus == WL_CONNECTED);
  screen_control::setLinkBars(screenCtx, wifiStatus == WL_CONNECTED ? 3 : 0);
  screen_control::setSsid(screenCtx, lastKnownNetwork);

  bool wifiJustConnected = (wifiStatus == WL_CONNECTED) && !lastWifiConnected;
  if (wifiJustConnected) {
    screen_control::recordConnection(screenCtx, nowMs);
  }
  lastWifiConnected = (wifiStatus == WL_CONNECTED);

  if (report.sen66Data.dataValid) {
    screen_control::setPm25(screenCtx, report.sen66Data.pm25, nowMs);
    screen_control::setTemperature(screenCtx, report.sen66Data.temperatureC, nowMs);
    screen_control::setHumidity(screenCtx, report.sen66Data.humidity);
    screen_control::setVoc(screenCtx, report.sen66Data.vocIndex);
    screen_control::setNox(screenCtx, report.sen66Data.noxIndex);
    screen_control::setCo2(screenCtx, static_cast<float>(report.sen66Data.co2ppm));
    screenCtx.device.sen66Ok = (report.sen66 != device_status::STATUS_ERROR);
  } else {
    screen_control::setPm25(screenCtx, 0.0f, nowMs);
    screen_control::setTemperature(screenCtx, 0.0f, nowMs);
    screen_control::setHumidity(screenCtx, 0.0f);
    screen_control::setVoc(screenCtx, 0.0f);
    screen_control::setNox(screenCtx, 0.0f);
    screen_control::setCo2(screenCtx, 0.0f);
    screenCtx.device.sen66Ok = false;
  }

  screenCtx.device.rtcOk = !report.rtcData.lostPower && report.rtcData.running;

  screenFlashStats.totalMB = report.flashData.totalMB;
  screenFlashStats.usedMB = report.flashData.usedMB;
  screenFlashStats.freeMB = report.flashData.freeMB;
  screenFlashStats.usedPercent = report.flashData.usedPercent;
  screenFlashStats.healthPercent = report.flashData.healthPercent;
  screenFlashStats.estimatedDaysLeft = report.flashData.estimatedDaysLeft;
  screenCtx.device.flashOk = (report.flash != device_status::STATUS_ERROR);

  screen_control::setWarning(screenCtx, report.rtcData.batteryLow, nowMs);

  if (state == RunState::Measure) {
    uint32_t remainingSec;
    if (measureWindowEndMsValid) {
      if (nowMs < measureWindowEndMs) {
        remainingSec = (measureWindowEndMs - nowMs) / 1000UL;
      } else {
        remainingSec = 0;
      }
    } else if (measureWindowEndValid) {
      DateTime rtcNow = rtc.now();
      if (rtcNow.year() >= 2020 && rtcNow.unixtime() > 0 && rtcNow.unixtime() < measureWindowEndUnix) {
        remainingSec = measureWindowEndUnix - rtcNow.unixtime();
      } else if (rtcNow.unixtime() >= measureWindowEndUnix) {
        remainingSec = 0;
      } else {
        uint32_t elapsed = nowMs - stateStartMs;
        remainingSec = (elapsed < runtimeCfg.activeDurationMs)
                           ? (runtimeCfg.activeDurationMs - elapsed) / 1000UL
                           : 0;
      }
    } else {
      uint32_t elapsed = nowMs - stateStartMs;
      remainingSec = (elapsed < runtimeCfg.activeDurationMs)
                         ? (runtimeCfg.activeDurationMs - elapsed) / 1000UL
                         : 0;
    }
    uint32_t minutes = remainingSec / 60UL;
    uint32_t seconds = remainingSec % 60UL;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
    screen_control::setJobTimeText(screenCtx, buf);
    logx::debugf("countdown", "job remaining %lu s (%s)",
                 static_cast<unsigned long>(remainingSec),
                 buf);
  } else {
    screen_control::clearJobTimeText(screenCtx);
    measureWindowEndValid = false;
    measureWindowEndMsValid = false;
  }

  screen_control::markDataDirty(screenCtx);
}

bool performMeasurement(uint32_t nowMs) {
  bool measurementAttempted = false;
  bool measurementSucceeded = false;

  uint32_t cadence = hasSuccessfulMeasurement ? runtimeCfg.measurementIntervalMs
                                              : runtimeCfg.refreshIntervalMs;
  if ((nowMs - lastMeasurementAttemptMs) >= cadence) {
    lastMeasurementAttemptMs = nowMs;
    measurementAttempted = true;
    measurementSucceeded = recordMeasurement(nowMs);
    if (measurementSucceeded) {
      hasSuccessfulMeasurement = true;
      lastMeasurementMs = nowMs;
    }
  }

  if ((nowMs - lastSummaryMs) >= runtimeCfg.summaryIntervalMs) {
    logSummary();
    lastSummaryMs = nowMs;
  }

  if (cloudConfig.enabled && flashLogger) {
    comms::cloud::publishIfDue(cloudConfig, cloudState, *flashLogger, nowMs);
  }

  bool windowElapsed = (nowMs - stateStartMs) >= runtimeCfg.activeDurationMs;
  if (measureWindowEndMsValid) {
    if (nowMs >= measureWindowEndMs) {
      windowElapsed = true;
    } else {
      windowElapsed = false;
    }
  }
  if (measureWindowEndValid) {
    DateTime rtcNow = rtc.now();
    if (rtcNow.year() >= 2020 && rtcNow.unixtime() > 0) {
      windowElapsed = rtcNow.unixtime() >= measureWindowEndUnix;
    }
  }
  if (windowElapsed && !hasSuccessfulMeasurement && !measurementWindowWarned) {
    if (!measurementAttempted) {
      recordMeasurement(nowMs);
    }
    logx::warn("measure", "active window elapsed without valid SEN66 sample");
    measurementWindowWarned = true;
  }

  return windowElapsed;
}

bool recordMeasurement(uint32_t nowMs) {
  if ((nowMs - lastRefreshMs) >= runtimeCfg.measurementIntervalMs) {
    refreshStatus(nowMs);
    lastRefreshMs = nowMs;
  }

  DateTime rtcNow = rtc.now();
  lastMeasurementUnix = rtcNow.unixtime();
  device_status::GlobalTime::update(rtcNow);

  float soc = batteryGauge.readPercent();
  float vbat = batteryGauge.readVoltage();
  if (!std::isnan(soc)) {
    report.batteryData.percent = soc;
    report.batteryData.dataValid = true;
    screen_control::setBatteryPercent(screenCtx,
                                      static_cast<uint8_t>(std::max(0, std::min(100, static_cast<int>(soc + 0.5f)))),
                                      nowMs);
  }
  if (!std::isnan(vbat)) {
    report.batteryData.voltage = vbat;
  }

  static uint32_t lastNoDataLogMs = 0;
  if (!report.sen66Data.dataValid) {
    if ((nowMs - lastNoDataLogMs) >= 5000) {
      logx::debug("measure", "SEN66 sample not ready yet");
      lastNoDataLogMs = nowMs;
    }
    return false;
  }

  logx::infof("measure",
              "sen66 pm1=%.2f pm25=%.2f pm10=%.2f temp=%.2fC rh=%.1f%% voc=%.2f nox=%.2f co2=%u",
              report.sen66Data.pm1,
              report.sen66Data.pm25,
              report.sen66Data.pm10,
              report.sen66Data.temperatureC,
              report.sen66Data.humidity,
              report.sen66Data.vocIndex,
              report.sen66Data.noxIndex,
              report.sen66Data.co2ppm);

  String logPayload;
  logPayload.reserve(160);
  logPayload += '{';
  logPayload += "\"ts\":";
  logPayload += String(lastMeasurementUnix);
  logPayload += ",\"pm1\":";
  logPayload += String(report.sen66Data.pm1, 2);
  logPayload += ",\"pm25\":";
  logPayload += String(report.sen66Data.pm25, 2);
  logPayload += ",\"pm10\":";
  logPayload += String(report.sen66Data.pm10, 2);
  logPayload += ",\"voc\":";
  logPayload += String(report.sen66Data.vocIndex, 2);
  logPayload += ",\"nox\":";
  logPayload += String(report.sen66Data.noxIndex, 2);
  logPayload += ",\"temp\":";
  logPayload += String(report.sen66Data.temperatureC, 2);
  logPayload += ",\"humidity\":";
  logPayload += String(report.sen66Data.humidity, 2);
  logPayload += ",\"battery_pct\":";
  logPayload += String(report.batteryData.percent, 1);
  logPayload += ",\"battery_v\":";
  logPayload += String(report.batteryData.voltage, 3);
  logPayload += ",\"rtc_temp\":";
  logPayload += String(report.rtcData.temperatureC, 2);
  logPayload += '}';

  if (flashLoggerReady && !flashLogger->append(logPayload)) {
    logx::warn("flash", "append failed");
  }
  updateFlashStats();
  return true;
}

bool performSync(uint32_t nowMs) {
  if (cloudConfig.enabled && flashLogger) {
    comms::cloud::publishIfDue(cloudConfig, cloudState, *flashLogger, nowMs);
  }

  if (!syncComplete && (nowMs - lastSyncAttemptMs) >= runtimeCfg.syncRetryIntervalMs) {
    lastSyncAttemptMs = nowMs;
    if (syncRtcFromNtp(rtc, timeSyncConfig, timeSyncState, nowMs)) {
      logx::info("time", "RTC synchronised with NTP");
      syncComplete = true;
    }
  }

  bool windowElapsed = (nowMs - stateStartMs) >= runtimeCfg.syncWindowMs;
  if (windowElapsed && !syncComplete) {
    logx::warn("time", "sync window elapsed without NTP success");
  }
  return syncComplete || windowElapsed;
}

void logSummary() {
  logx::infof("summary",
              "sen66 status=%s dataValid=%s pm25=%.2fug/m3 temp=%.1fC rh=%.1f%%",
              device_status::statusToString(report.sen66),
              report.sen66Data.dataValid ? "yes" : "no",
              report.sen66Data.dataValid ? report.sen66Data.pm25 : 0.0f,
              report.sen66Data.dataValid ? report.sen66Data.temperatureC : 0.0f,
              report.sen66Data.dataValid ? report.sen66Data.humidity : 0.0f);

  logx::infof("summary",
              "rtc status=%s running=%s lostPower=%s temp=%.1fC unix=%lu",
              device_status::statusToString(report.rtc),
              report.rtcData.running ? "yes" : "no",
              report.rtcData.lostPower ? "yes" : "no",
              report.rtcData.temperatureC,
              static_cast<unsigned long>(report.rtcData.unixTime));

  logx::infof("summary",
              "wifi status=%s",
              WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");

  logx::infof("summary",
              "flash status=%s detail=%s selfTest=%s used=%.1f%% health=%.1f%% daysLeft=%.1f",
              device_status::statusToString(report.flash),
              flashProvider.details(),
              report.flashData.selfTestPassed ? "ok" : "fail",
              report.flashData.usedPercent,
              report.flashData.healthPercent,
              report.flashData.estimatedDaysLeft);
}

void prepareForSleep() {
  logx::info("main", "preparing for sleep");
  manager.refreshAll();
  disconnectWifi();
  disableAutoScreens(millis());
  screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::DeviceStatus, millis());
  screenManager.setDataAreaEnabled(true);
  screenManager.setStatusFramesEnabled(true);
  screenManager.setData(0, 0, String(F("Sleep mode")));
  screenManager.setData(1, 0, String(F("Press button to wake")));
  screenManager.setData(2, 0, String());
  screenManager.setData(3, 0, String());
  screenManager.setData(4, 0, String());
  screen_control::markDataDirty(screenCtx);
  screen_control::tick(screenCtx, millis());
  logx::info("main", "sleep screen rendered");
  delay(500);
  oledDisplay.clearBuffer();
  oledDisplay.sendBuffer();
  oledDisplay.setPowerSave(1);
  Serial.flush();
  delay(50);
}

void enterSleep(uint32_t sleepMs) {
  logx::infof("main", "entering sleep ms=%lu", static_cast<unsigned long>(sleepMs));
#if defined(ARDUINO_ARCH_ESP32)
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sleepMs) * 1000ULL);
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(kButtonPin), 0);
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
#else
  delay(sleepMs);
#endif
}

void updateAutoScreens(uint32_t nowMs) {
  if (!autoScreenEnabled || state != RunState::Measure) {
    return;
  }

  uint32_t elapsed = nowMs - autoScreenLastSwitchMs;
  switch (autoScreenState) {
    case AutoScreenState::Graph:
      if (screenCtx.activeScreen != screen_control::ScreenId::Particulate) {
        screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::Particulate, nowMs);
        logx::info("auto-screen", "forcing particulate screen");
      }
      if (elapsed >= runtimeCfg.graphScreenMs) {
        autoScreenState = AutoScreenState::Feeling;
        autoScreenLastSwitchMs = nowMs;
        screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::IndoorComfort, nowMs);
        logx::info("auto-screen", "switching to indoor comfort screen");
      }
      break;
    case AutoScreenState::Feeling:
      if (screenCtx.activeScreen != screen_control::ScreenId::IndoorComfort) {
        screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::IndoorComfort, nowMs);
        logx::info("auto-screen", "forcing indoor comfort screen");
      }
      if (elapsed >= runtimeCfg.feelingScreenMs) {
        autoScreenState = AutoScreenState::Graph;
        autoScreenLastSwitchMs = nowMs;
        screen_control::setActiveScreen(screenCtx, screen_control::ScreenId::Particulate, nowMs);
        logx::info("auto-screen", "switching back to particulate screen");
      }
      break;
  }
}

void disableAutoScreens(uint32_t nowMs) {
  autoScreenEnabled = false;
  autoScreenLastSwitchMs = nowMs;
}

void updateFlashStats() {
  if (!flashLogger) return;
  FlashStats stats = flashLogger->getFlashStats(3600.0f);
  report.flashData.totalMB = stats.totalMB;
  report.flashData.usedMB = stats.usedMB;
  report.flashData.freeMB = stats.freeMB;
  report.flashData.usedPercent = stats.usedPercent;
  report.flashData.healthPercent = stats.healthPercent;
  report.flashData.estimatedDaysLeft = stats.estimatedDaysLeft;

  uint16_t usedSectors = static_cast<uint16_t>((stats.usedMB * 1024.0f * 1024.0f) / SECTOR_SIZE);
  flash.simulateUsage(usedSectors, 0, 0.0f);
}
}  // namespace
