#include <Wire.h>
#include <RTClib.h>

#include "modules/ds3231/include/Ds3231Clock.h"

Ds3231Clock rtc;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire.begin();

  if (!rtc.begin(Wire)) {
    Serial.println(F("RTC init failed"));
    while (true) { delay(1000); }
  }

  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, seeding demo time"));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println(F("RTC ready"));
}

void loop() {
  DateTime now = rtc.now();
  Serial.print(F("Unix time: "));
  Serial.print(now.unixtime());
  Serial.print(F(", Temp: "));
  Serial.print(rtc.temperatureC(), 1);
  Serial.println(F(" C"));
  delay(1000);
}

