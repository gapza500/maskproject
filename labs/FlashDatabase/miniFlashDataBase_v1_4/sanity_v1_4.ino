#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.4 sanity");

  Wire.begin(19, 20);
  if (!rtc.begin()) {
    Serial.println("RTC missing");
    while (1) delay(1000);
  }

  logger.begin(&rtc);
  logger.setFactoryInfo("TestDevice", "W25Q64JV", "000000000001");
  logger.setDateStyle(2); // ISO

  logger.append("{\\"temp\\":25.5,\\"hum\\":60,\\"bat\\":88}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61,\\"bat\\":88}");

  logger.printFormattedLogs();
  FlashStats fs = logger.getFlashStats(3500.0f);
  Serial.printf("Stats: total=%.2fMB used=%.2fMB free=%.2fMB health=%.1f%% estDays=%u\n",
                fs.totalMB, fs.usedMB, fs.freeMB, fs.healthPercent, fs.estimatedDaysLeft);
  logger.printFactoryInfo();
}

void loop() {}
