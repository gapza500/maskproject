#include <Wire.h>
#include <SensirionI2cSen66.h>
#include <RTClib.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_sleep.h>
#endif

#define SCREEN_BACKEND_U8G2
#include <U8g2lib.h>
#include "../../modules/screen_manager/include/screen_config.h"
#include "../../modules/screen_manager/include/screen_manager.h"

#include "device_status.h"
#include "include/device_status/providers/Sen66StatusProvider.h"
#include "include/device_status/providers/Max17048StatusProvider.h"
#include "include/device_status/providers/Ds3231StatusProvider.h"
#include "include/device_status/providers/OledStatusProvider.h"
#include "include/device_status/providers/FlashStatusProvider.h"
#include "include/device_status/GlobalTime.h"

namespace {
// Runtime scheduling windows
constexpr uint32_t kActiveDurationMs = 3UL * 60UL * 1000UL;   // 3 minutes
constexpr uint32_t kSleepDurationMs  = 12UL * 60UL * 1000UL;  // 12 minutes
constexpr uint32_t kRefreshIntervalMs = 1000UL;               // status refresh cadence

// Hardware objects
SensirionI2cSen66 sen66Driver;
Max17048 batteryGauge;
Ds3231Clock rtc;
FlashStore flash;
screen_manager::ScreenManager screenManager;
U8G2_SSD1309_128X64_NONAME0_F_SW_I2C oledDisplay(U8G2_R0, /* clock=*/22, /* data=*/21, /* reset=*/U8X8_PIN_NONE);

// Shared status state
device_status::DeviceStatusReport report;
device_status::DeviceStatusManager manager(report);

// Providers
device_status::Sen66StatusProvider sen66Provider(sen66Driver, Wire);
device_status::Max17048StatusProvider batteryProvider(batteryGauge, Wire);
device_status::Ds3231StatusProvider rtcProvider(rtc);
device_status::OledStatusProvider oledProvider(screenManager);
device_status::FlashStatusProvider flashProvider(flash);

// Timing helpers
uint32_t stateStartMs = 0;
uint32_t lastRefreshMs = 0;
uint32_t lastSummaryMs = 0;
constexpr uint32_t kSummaryIntervalMs = 10UL * 1000UL;  // print every 10s during active window

enum class RunState : uint8_t { Active, SleepPending };
RunState state = RunState::Active;

void printSummary();
void prepareForSleep();
void enterSleep(uint32_t sleepMs);
}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire.begin();
  Wire.setClock(100000);  // SEN66 requires 100 kHz

  oledDisplay.begin();
  if (!screenManager.begin(oledDisplay)) {
    Serial.println(F("[status] screen manager init failed"));
  } else {
    screenManager.setDataAreaEnabled(true);
    screenManager.setDataFramesEnabled(false);
    screenManager.setStatusFramesEnabled(false);
    screenManager.markAllDirty();
  }

  oledProvider.setHeartbeatMessage("Device Status");
  oledProvider.setStatusText(String("SEN66: --"), String("BAT: --"));
  flashProvider.enableSelfTest(false);
  flash.simulateUsage(/*usedSectors=*/512, /*totalEraseOps=*/4000, /*avgBytesPerDay=*/128 * 1024.0f);

  manager.addProvider(sen66Provider);
  manager.addProvider(batteryProvider);
  manager.addProvider(rtcProvider);
  manager.addProvider(oledProvider);
  manager.addProvider(flashProvider);

  if (!manager.beginAll()) {
    Serial.println(F("[status] init warning: some providers failed setup"));
  }

  stateStartMs = millis();
  lastRefreshMs = stateStartMs;
  lastSummaryMs = stateStartMs;
}

void loop() {
  const uint32_t now = millis();

  switch (state) {
    case RunState::Active: {
      if ((now - lastRefreshMs) >= kRefreshIntervalMs) {
        manager.refreshAll();
        lastRefreshMs = now;
      }
      if ((now - lastSummaryMs) >= kSummaryIntervalMs) {
        printSummary();
        lastSummaryMs = now;
      }
      if ((now - stateStartMs) >= kActiveDurationMs) {
        state = RunState::SleepPending;
      }
      break;
    }

    case RunState::SleepPending: {
      printSummary();
      prepareForSleep();
      enterSleep(kSleepDurationMs);
      oledProvider.setHeartbeatMessage("Device Status");
      oledProvider.setStatusText(String("SEN66: --"), String("BAT: --"));
      state = RunState::Active;
      stateStartMs = millis();
      lastRefreshMs = stateStartMs;
      lastSummaryMs = stateStartMs;
      break;
    }
  }
}

namespace {

void printSummary() {
  Serial.println();
  Serial.println(F("=== Device Status Summary ==="));

  Serial.print(F("SEN66   : "));
  Serial.println(device_status::statusToString(report.sen66));
  Serial.print(F("  detail: "));
  Serial.println(sen66Provider.details());
  if (report.sen66Data.dataValid) {
    Serial.print(F("  PM2.5 : "));
    Serial.print(report.sen66Data.pm25, 2);
    Serial.print(F(" ug/m3, Temp: "));
    Serial.print(report.sen66Data.temperatureC, 1);
    Serial.print(F(" C, RH: "));
    Serial.print(report.sen66Data.humidity, 1);
    Serial.println(F(" %"));
  }

  Serial.print(F("Battery : "));
  Serial.println(device_status::statusToString(report.battery));
  Serial.print(F("  detail: "));
  Serial.println(batteryProvider.details());
  if (report.batteryData.dataValid) {
    Serial.print(F("  %: "));
    Serial.print(report.batteryData.percent, 1);
    Serial.print(F(", Vbat: "));
    Serial.println(report.batteryData.voltage, 3);
  }

  Serial.print(F("RTC     : "));
  Serial.println(device_status::statusToString(report.rtc));
  Serial.print(F("  detail: "));
  Serial.println(rtcProvider.details());
  Serial.print(F("  running="));
  Serial.print(report.rtcData.running ? F("yes") : F("no"));
  Serial.print(F(", lostPower="));
  Serial.print(report.rtcData.lostPower ? F("yes") : F("no"));
  Serial.print(F(", temp "));
  Serial.print(report.rtcData.temperatureC, 1);
  Serial.print(F(" C"));
  Serial.print(F(", unix="));
  Serial.println(report.rtcData.unixTime);
  if (device_status::GlobalTime::valid()) {
    DateTime dt = device_status::GlobalTime::current();
    Serial.print(F("  GlobalTime: "));
    Serial.println(dt.timestamp(DateTime::TIMESTAMP_FULL));
  }

  Serial.print(F("OLED    : "));
  Serial.println(device_status::statusToString(report.oled));
  Serial.print(F("  detail: "));
  Serial.println(oledProvider.details());

  Serial.print(F("Flash   : "));
  Serial.println(device_status::statusToString(report.flash));
  Serial.print(F("  detail: "));
  Serial.println(flashProvider.details());
  Serial.print(F("  selfTest="));
  Serial.println(report.flashData.selfTestPassed ? F("ok") : F("fail"));
  Serial.print(F("  usage: "));
  Serial.print(report.flashData.usedPercent, 1);
  Serial.print(F("% of "));
  Serial.print(report.flashData.totalMB, 2);
  Serial.print(F(" MB, health "));
  Serial.print(report.flashData.healthPercent, 1);
  Serial.print(F("%, est days "));
  Serial.println(report.flashData.estimatedDaysLeft);

  String line1 = String("SEN66 ") + String(device_status::statusToString(report.sen66)) +
                  String(" / BAT ") + String(device_status::statusToString(report.battery));
  String line2 = String("RTC ") + String(device_status::statusToString(report.rtc)) +
                  String(" / FLASH ") + String(device_status::statusToString(report.flash));
  oledProvider.setStatusText(line1, line2);
}

void prepareForSleep() {
  Serial.println(F("[status] preparing for sleep..."));
  manager.refreshAll();  // grab a fresh snapshot before going to sleep
  oledProvider.setHeartbeatMessage("Sleeping...");
  oledProvider.setStatusText(String(F("Sleep mode")), String());
  oledProvider.refresh(report);  // push message before sleep
  // TODO: stop active sensors / persist last measurements before deep sleep
}

void enterSleep(uint32_t sleepMs) {
  Serial.print(F("[status] entering sleep for ms="));
  Serial.println(sleepMs);

#if defined(ARDUINO_ARCH_ESP32)
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sleepMs) * 1000ULL);
  esp_deep_sleep_start();
#else
  delay(sleepMs);
#endif
}

}  // namespace
