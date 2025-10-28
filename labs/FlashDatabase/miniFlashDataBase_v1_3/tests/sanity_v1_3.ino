#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.3 sanity");

  Wire.begin(19, 20);
  if (!rtc.begin()) {
    Serial.println("RTC missing");
    while (1) delay(1000);
  }

  logger.begin(&rtc);
  logger.setDateStyle(1); // Thai

  logger.append("{\\"temp\\":25.5,\\"hum\\":60,\\"bat\\":88}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61,\\"bat\\":88}");
  logger.append("{\\"temp\\":26.0,\\"hum\\":62,\\"bat\\":87}");

  logger.printFormattedLogs();
  logger.readAll();
}

void loop() {}
