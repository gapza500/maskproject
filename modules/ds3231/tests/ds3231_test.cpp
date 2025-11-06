#ifndef ARDUINO
int main() { return 0; }
#else
#include <Wire.h>
#include <RTClib.h>

#include "../include/Ds3231Clock.h"

int main() {
  Wire.begin();
  Ds3231Clock rtc;
  if (!rtc.begin(Wire)) {
    return 1;
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  (void)rtc.now();
  return 0;
}
#endif
