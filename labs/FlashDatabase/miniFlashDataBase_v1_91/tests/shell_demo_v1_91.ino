#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

String readLine(Stream& s) {
  static String buf;
  while (s.available()) {
    char c = (char)s.read();
    if (c == '\r') continue;
    if (c == '\n') { String out = buf; buf = ""; return out; }
    buf += c;
  }
  return "";
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.91 shell demo");

  Wire.begin(19, 20);
  rtc.begin();

  logger.begin(&rtc);
  logger.append("{\\"temp\\":25.5,\\"hum\\":60}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61}");
  logger.append("{\\"temp\\":26.0,\\"hum\\":62}");

  Serial.println("Type: ls / ls sectors / cd day <date> / print / info / etc.");
}

void loop() {
  String cmd = readLine(Serial);
  if (cmd.length()) {
    if (!logger.handleCommand(cmd, Serial)) {
      Serial.println("unknown command");
    }
  }
}
