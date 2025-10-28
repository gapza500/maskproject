#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.6 sanity");

  Wire.begin(19, 20);
  if (!rtc.begin()) {
    Serial.println("RTC missing");
    while (1) delay(1000);
  }

  logger.begin(&rtc);
  logger.setFactoryInfo("TestDevice", "W25Q64JV", "000000000002");
  logger.setDateStyle(1);

  logger.append("{\\"temp\\":25.5,\\"hum\\":60,\\"bat\\":88}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61,\\"bat\\":88}");

  Serial.println(F("Type commands: pf | stats | factory"
                   " | reset <code> | mark | gc"));
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (cmd.equalsIgnoreCase("pf")) {
      logger.printFormattedLogs();
    } else if (cmd.equalsIgnoreCase("stats")) {
      FlashStats fs = logger.getFlashStats(3500.0f);
      Serial.printf("Total=%.2fMB Used=%.2fMB Free=%.2fMB Used=%.1f%% Health=%.1f%% EstDays=%u\n",
                    fs.totalMB, fs.usedMB, fs.freeMB, fs.usedPercent, fs.healthPercent, fs.estimatedDaysLeft);
    } else if (cmd.equalsIgnoreCase("factory")) {
      logger.printFactoryInfo();
    } else if (cmd.startsWith("reset")) {
      String rest = cmd.substring(5); rest.trim();
      bool ok = logger.factoryReset(rest);
      Serial.println(ok ? "Factory reset OK" : "Factory reset FAILED");
    } else if (cmd.equalsIgnoreCase("mark")) {
      logger.markCurrentDayPushed();
      Serial.println("Marked current day pushed");
    } else if (cmd.equalsIgnoreCase("gc")) {
      logger.gc();
      Serial.println("GC run");
    }
  }
}
