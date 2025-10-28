#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.8 sanity");

  Wire.begin(19, 20);
  if (!rtc.begin()) {
    Serial.println("RTC missing");
    while (1) delay(1000);
  }

  logger.begin(&rtc);
  logger.setFactoryInfo("TestDevice", "W25Q64JV", "000000000004");
  logger.setDateStyle(2);

  logger.append("{\\"temp\\":25.5,\\"hum\\":60,\\"bat\\":88}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61,\\"bat\\":88}");
  logger.append("{\\"temp\\":26.0,\\"hum\\":62,\\"bat\\":87}");

  logger.printFormattedLogs();
  FlashStats fs = logger.getFlashStats(3500.0f);
  Serial.printf("Total=%.2fMB Used=%.2fMB Free=%.2fMB Used=%.1f%% Health=%.1f%% EstDays=%u Low=%s\n",
                fs.totalMB, fs.usedMB, fs.freeMB, fs.usedPercent, fs.healthPercent, fs.estimatedDaysLeft,
                logger.isLowSpace() ? "YES":"NO");

  logger.printFactoryInfo();
  logger.markCurrentDayPushed();
  logger.gc();
}

void loop() {}
