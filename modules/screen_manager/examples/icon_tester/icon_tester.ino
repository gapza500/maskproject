#define SCREEN_BACKEND_U8G2

#include <Wire.h>
#include <U8g2lib.h>

#include "../../../dfrobot_beetle_esp32c6_mini/pins_dfrobot_beetle_esp32c6_mini.h"
#include "../../include/screen_config.h"
#include "../../include/screen_manager.h"

using board_pins::dfrobot_beetle_esp32c6_mini::I2C_SCL_PIN;
using board_pins::dfrobot_beetle_esp32c6_mini::I2C_SDA_PIN;

namespace {

screen_manager::ScreenManager screen;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);

uint32_t lastStepMs = 0;
bool wifiOn = true;
bool warnOn = false;
bool btOn = true;
uint8_t linkBars = 0;
uint8_t batteryPercent = 80;

screen_config::IconId batteryIconFor(uint8_t percent) {
  if (percent <= 10) return screen_config::IconId::BatteryEmpty;
  if (percent <= 35) return screen_config::IconId::BatteryQuarter;
  if (percent <= 65) return screen_config::IconId::BatteryHalf;
  if (percent <= 90) return screen_config::IconId::BatteryThreeQuarter;
  return screen_config::IconId::BatteryFull;
}

screen_config::IconId signalIconFor(uint8_t bars) {
  switch (bars) {
    case 0:
      return screen_config::IconId::Signal0;
    case 1:
      return screen_config::IconId::Signal1;
    case 2:
      return screen_config::IconId::Signal2;
    default:
      return screen_config::IconId::Signal3;
  }
}

void updateStatusIcons() {
  screen.setStatusIcon(0, wifiOn ? screen_config::IconId::WifiOn
                                 : screen_config::IconId::WifiOff);
  screen.setStatusIcon(1, btOn ? screen_config::IconId::Bluetooth
                               : screen_config::IconId::Dot);
  screen.setStatusIcon(2, signalIconFor(linkBars));
  screen.setStatusIcon(3, warnOn ? screen_config::IconId::Warning
                                 : screen_config::IconId::Dot);
  screen.setStatusIcon(4, batteryIconFor(batteryPercent));
  screen.setStatusValueVisible(4, true);
  screen.setStatusValue(4, batteryPercent);
}

void printHelp() {
  Serial.println(F("\nIcon tester commands:"));
  Serial.println(F("  wifi on|off"));
  Serial.println(F("  bt on|off"));
  Serial.println(F("  link 0-3"));
  Serial.println(F("  warn on|off"));
  Serial.println(F("  batt <0-100>"));
  Serial.println(F("  help"));
}

void handleCommand(const String& cmd) {
  if (cmd.startsWith("wifi")) {
    wifiOn = cmd.indexOf("off") == -1;
  } else if (cmd.startsWith("bt")) {
    btOn = cmd.indexOf("off") == -1;
  } else if (cmd.startsWith("link")) {
    int value = cmd.substring(4).toInt();
    linkBars = constrain(value, 0, 3);
  } else if (cmd.startsWith("warn")) {
    warnOn = cmd.indexOf("off") == -1;
  } else if (cmd.startsWith("batt")) {
    int value = cmd.substring(4).toInt();
    batteryPercent = constrain(value, 0, 100);
  } else if (cmd == "help") {
    printHelp();
  } else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd);
  }
  updateStatusIcons();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("SSD1309 status icon tester"));

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, screen_config::kI2cClockHz);

  if (!screen.begin(u8g2)) {
    Serial.println(F("Screen manager failed to initialise"));
    while (true) {
      delay(1000);
    }
  }

  screen.setStatusFramesEnabled(false);
  screen.setDataAreaEnabled(false);
  screen.setCustomRenderer(nullptr);

  screen.setStatusLabel(4, "BAT");
  updateStatusIcons();
  printHelp();
}

void loop() {
  uint32_t now = millis();

  if (now - lastStepMs >= 1500) {
    wifiOn = !wifiOn;
    warnOn = !warnOn;
    linkBars = (linkBars + 1) % 4;
    batteryPercent = (batteryPercent + 10) % 110;
    if (batteryPercent > 100) batteryPercent = 0;
    updateStatusIcons();
    lastStepMs = now;
  }

  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (!cmd.isEmpty()) {
      handleCommand(cmd);
    }
  }

  screen.tick(now);
  screen.render(now);
}

