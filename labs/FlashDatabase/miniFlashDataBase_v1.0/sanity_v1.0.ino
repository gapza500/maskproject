#include <Arduino.h>
#include "FlashLogger.h"

FlashLogger logger(4);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nv1.0 sanity");

  logger.begin();
  logger.append("{\\"temp\\":25.5,\\"hum\\":60}");
  logger.append("{\\"temp\\":25.7,\\"hum\\":61}");
  logger.append("{\\"temp\\":26.0,\\"hum\\":62}");

  Serial.println("listing sectors...");
  logger.listSectors();

  Serial.println("dumping logs...");
  logger.readAll();
  Serial.println("done");
}

void loop() {}
