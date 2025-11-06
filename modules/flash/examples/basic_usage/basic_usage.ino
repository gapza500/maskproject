#include "modules/flash/include/FlashStore.h"

FlashStore flash;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  if (!flash.begin()) {
    Serial.println(F("Flash init failed"));
    while (true) { delay(1000); }
  }

  const char* msg = "hello flash";
  flash.writeRecord(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
  Serial.println(F("Wrote test record"));
}

void loop() {
  uint8_t buffer[32];
  size_t len = flash.readLatest(buffer, sizeof(buffer));

  Serial.print(F("Read "));
  Serial.print(len);
  Serial.print(F(" bytes: "));
  for (size_t i = 0; i < len; ++i) {
    Serial.write(buffer[i]);
  }
  Serial.println();
  delay(2000);
}

