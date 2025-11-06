#include <Arduino.h>

constexpr int kButtonPin = 15;  // Connect button between 3.3V and GPIO 15

int lastState = LOW;
unsigned long lastTransitionMs = 0;
constexpr unsigned long kDebounceMs = 30;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== Button Test v2 ==="));
  Serial.println(F("Wiring: GPIO15 <-> Button <-> 3.3V (internal pulldown enabled)"));
  Serial.println(F("Expect LOW at idle, HIGH when pressed.\n"));

  pinMode(kButtonPin, INPUT_PULLDOWN);  // idle LOW, goes HIGH on press
  lastState = digitalRead(kButtonPin);
}

void loop() {
  int current = digitalRead(kButtonPin);
  unsigned long now = millis();

  if (current != lastState) {
    if ((now - lastTransitionMs) > kDebounceMs) {
      lastTransitionMs = now;
      if (current == HIGH) {
        Serial.println(F("Button PRESSED (HIGH)"));
      } else {
        Serial.println(F("Button RELEASED (LOW)"));
      }
      lastState = current;
    }
  }
}
