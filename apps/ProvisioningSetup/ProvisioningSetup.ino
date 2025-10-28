#include "../../labs/ProvisioningManager/ProvisioningManager.h"
#include "../../labs/ProvisioningManager/ProvisioningManager.cpp"  // Inline compile for Arduino builder.

namespace {
ProvisioningConfig provisioningConfig = {
    "Beetle-C6-Setup",
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

ProvisioningManager provisioningManager(provisioningConfig);
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nProvisioning setup start");

    provisioningManager.begin();
}

void loop() {
    provisioningManager.loop();
}
