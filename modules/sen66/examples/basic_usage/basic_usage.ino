#include <Wire.h>

#include "modules/sen66/include/Sen66Driver.h"

Sen66Driver sen66;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire.begin();
  Wire.setClock(100000);  // SEN66 expects 100 kHz I2C

  if (!sen66.begin()) {
    Serial.println(F("SEN66 init failed"));
    while (true) { delay(1000); }
  }

  Serial.println(F("SEN66 ready"));
}

void loop() {
  if (sen66.readOnce()) {
    Serial.print(F("PM2.5: "));
    Serial.print(sen66.pm25(), 1);
    Serial.print(F(" ug/m3, Temp: "));
    Serial.print(sen66.temperature(), 1);
    Serial.print(F(" C, RH: "));
    Serial.print(sen66.humidity(), 1);
    Serial.println(F(" %"));
  } else {
    Serial.println(F("SEN66 read failed"));
  }

  delay(1000);
}

