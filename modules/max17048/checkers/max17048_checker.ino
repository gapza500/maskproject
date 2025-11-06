#include <Wire.h>

#include "modules/max17048/Max17048.h"

Max17048 gauge;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire.begin();

  Serial.println(F("=== MAX17048 Diagnostic Checker ==="));

  if (!gauge.begin()) {
    Serial.println(F("[ERR] MAX17048 not responding"));
    while (true) { delay(1000); }
  }

  Serial.println(F("[OK] MAX17048 initialised."));
}

void loop() {
  float soc = gauge.readPercent();
  float vbat = gauge.readVoltage();

  if (isnan(soc) || isnan(vbat)) {
    Serial.println(F("[ERR] reading failed"));
  } else {
    Serial.print(F("[OK] SOC "));
    Serial.print(soc, 2);
    Serial.print(F(" %, Vbat "));
    Serial.print(vbat, 3);
    Serial.println(F(" V"));
  }

  delay(1000);
}

