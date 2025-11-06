#include "modules/flash/include/FlashStore.h"

FlashStore flash;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println(F("=== W25Q128 Flash Checker ==="));

  if (!flash.begin()) {
    Serial.println(F("[ERR] flash.begin() failed"));
    while (true) { delay(1000); }
  }

  Serial.println(F("[OK] flash initialised"));
}

void loop() {
  const uint8_t pattern[] = {0xAA, 0x55, 0xFF, 0x00};
  bool writeOk = flash.writeRecord(pattern, sizeof(pattern));
  uint8_t readback[sizeof(pattern)];
  size_t read = flash.readLatest(readback, sizeof(readback));

  if (!writeOk || read != sizeof(pattern)) {
    Serial.println(F("[ERR] self-test failed"));
  } else {
    Serial.println(F("[OK] self-test passed"));
  }

  delay(1000);
}

