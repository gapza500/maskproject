#include <Wire.h>

#include "modules/max17048/Max17048.h"

Max17048 gauge;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire.begin();

  if (!gauge.begin()) {
    Serial.println(F("MAX17048 not detected. Check wiring!"));
    while (true) { delay(1000); }
  }

  Serial.println(F("MAX17048 ready"));
}

void loop() {
  float soc = gauge.readPercent();
  float vbat = gauge.readVoltage();

  Serial.print(F("SOC: "));
  Serial.print(soc, 1);
  Serial.print(F(" %, Vbat: "));
  Serial.print(vbat, 3);
  Serial.println(F(" V"));

  delay(2000);
}

