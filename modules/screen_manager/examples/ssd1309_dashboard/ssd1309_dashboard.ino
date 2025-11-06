#define SCREEN_BACKEND_U8G2

#include <Wire.h>
#include <U8g2lib.h>
#include <string.h>

#include "../../../../apps/ssd1309_dashboard/include/screen_control.h"
#include "../../../../apps/ssd1309_dashboard/include/screen_serial.h"
#include "../../../esp32_devkit_v1/pins_esp32_devkit_v1.h"
#include "../../../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.h"

using board_pins::esp32_devkit_v1::I2C_SCL_PIN;
using board_pins::esp32_devkit_v1::I2C_SDA_PIN;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);
screen_manager::ScreenManager screen;
dashboard_view::DashboardView dashboardView;
Ds3231Clock rtc;
FlashStats flashStats{};
FactoryInfo factoryInfo{};
screen_control::ControlContext ctx;

void seedFlashStats() {
  flashStats.totalMB = (MAX_SECTORS - 1) * (SECTOR_SIZE / 1024.0f / 1024.0f);
  flashStats.usedMB = flashStats.totalMB * 0.34f;
  flashStats.healthPercent = 92.0f;
  flashStats.freeMB = flashStats.totalMB - flashStats.usedMB;
  flashStats.usedPercent = (flashStats.totalMB > 0.0f)
                               ? (flashStats.usedMB / flashStats.totalMB) * 100.0f
                               : 0.0f;
  flashStats.estimatedDaysLeft = 180;
}

void seedFactoryInfo() {
  factoryInfo.magic = 0x46414354UL;
  strncpy(factoryInfo.model, "miniFlashDB v2.0", sizeof(factoryInfo.model) - 1);
  strncpy(factoryInfo.flashModel, "W25Q64JV", sizeof(factoryInfo.flashModel) - 1);
  strncpy(factoryInfo.deviceId, "107438112233", sizeof(factoryInfo.deviceId) - 1);
  factoryInfo.model[sizeof(factoryInfo.model) - 1] = '\0';
  factoryInfo.flashModel[sizeof(factoryInfo.flashModel) - 1] = '\0';
  factoryInfo.deviceId[sizeof(factoryInfo.deviceId) - 1] = '\0';
  factoryInfo.totalEraseOps = 1240;
  factoryInfo.bootCounter = 128;
  factoryInfo.badCount = 1;
  factoryInfo.badList[0] = 12;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("SSD1309 dashboard example starting..."));

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, screen_config::kI2cClockHz);

  rtc.begin();
  rtc.setUnixTime(screen_control::kBaseEpoch);
  rtc.setTemperatureC(25.0f);

  seedFlashStats();
  seedFactoryInfo();

  if (!screen.begin(u8g2)) {
    Serial.println(F("Failed to start screen manager"));
    while (true) {
      delay(1000);
    }
  }

  dashboardView.begin(screen);
  screen.setDataAreaEnabled(true);
  screen.setDataFramesEnabled(false);
  screen.setStatusFramesEnabled(false);

  screen_control::initialize(ctx, screen, dashboardView, rtc,
                             flashStats, factoryInfo,
                             screen_control::ScreenId::Dashboard, millis());

  screen_control::setModel(ctx, String(factoryInfo.model));
  screen_control::setUser(ctx, String(factoryInfo.deviceId));
  screen_control::recordConnection(ctx, millis());

  screen_serial::printHelp();
}

void loop() {
  uint32_t now = millis();

  screen_control::updateDashboard(ctx, now);

  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.isEmpty()) {
      continue;
    }
    cmd.toLowerCase();
    if (!screen_serial::handleCommand(ctx, cmd, now)) {
      Serial.print(F("Unknown command: "));
      Serial.println(cmd);
    }
  }

  screen_control::tick(ctx, now);
}
