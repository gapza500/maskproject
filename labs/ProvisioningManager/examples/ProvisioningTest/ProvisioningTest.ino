#include "../../ProvisioningManager.h"
#include "../../ProvisioningManager.cpp"  // Compile implementation within this sketch for convenience.

namespace {
ProvisioningConfig config = {
    "Test-Setup",
    "password123",
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0),
    15,
    15000,
    500,
    1000,
    10000,
    2000,
    4210
};

ProvisioningManager manager(config);
unsigned long lastStatusPrint = 0;
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("ProvisioningManager demo");
    Serial.println("- Connect to the AP SSID above");
    Serial.println("- Open http://192.168.4.1 to provision");
    Serial.println("- Type 'reset' in Serial Monitor to clear credentials");

    manager.begin();
}

void loop() {
    manager.loop();

    if (millis() - lastStatusPrint >= 5000) {
        lastStatusPrint = millis();
        Serial.print("Mode: ");
        Serial.print(manager.isProvisioning() ? "AP" : "STA");
        Serial.print(" | Last network: ");
        const String& last = manager.lastProvisionedNetwork();
        Serial.println(last.length() ? last : "(none yet)");
    }
}
