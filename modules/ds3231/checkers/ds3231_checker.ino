#include <Wire.h>
#include <RTClib.h>

#include "modules/ds3231/include/Ds3231Clock.h"

Ds3231Clock rtc;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println(F("=== DS3231 Diagnostic Checker ==="));

  Wire.begin();

  if (!rtc.begin(Wire)) {
    Serial.println(F("[ERR] rtc.begin() failed"));
    while (true) { delay(1000); }
  }

  if (rtc.lostPower()) {
    Serial.println(F("[WARN] RTC reports power loss"));
  } else {
    Serial.println(F("[OK] RTC running"));
  }
}

void loop() {
  DateTime now = rtc.now();
  Serial.print(F("Unix time: "));
  Serial.print(now.unixtime());
  Serial.print(F(" Temp: "));
  Serial.print(rtc.temperatureC(), 1);
  Serial.println(F(" C"));
  delay(2000);
}

