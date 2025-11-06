# ProvisioningManager Source Bundle

This directory contains the reusable provisioning component intended for inclusion in your main control sketches.

## Files
- `ProvisioningManager.h` – Public API, configuration struct, and class definition.
- `ProvisioningManager.cpp` – Implementation of the provisioning workflow.

The module provides:
- Captive-portal access point hosting (`/`, `/scan`, `/save`, `/status`).
- Credential persistence via `Preferences`.
- Automatic transition to STA mode with reconnect/backoff handling.
- UDP broadcast (`event: "provisioning_complete"`) so companion apps can detect success.
- Serial `reset` command to wipe credentials and restart provisioning.

## Using From a Sketch
```cpp
#include "../ProvisioningManager/ProvisioningManager.h"
#include "../ProvisioningManager/ProvisioningManager.cpp"  // only while prototype; copy files locally for production.

ProvisioningConfig cfg = {
    "My-Setup-AP",
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

ProvisioningManager provisioning(cfg);

void setup() {
    Serial.begin(115200);
    provisioning.begin();
}

void loop() {
    provisioning.loop();
}
```

> **Note:** During development you can include the `.cpp` directly as shown. For production sketches copy both files into the sketch folder (or promote them to a real Arduino library) and drop the explicit `.cpp` include so the builder picks it up automatically.

## Related Resources
- `labs/ProvisioningManager/` – Duplicate copy kept for lab experiments plus a demo sketch and cheat sheet.
- `labs/APwifi/APwifi.ino` – Original monolithic provisioning experiment retained for reference.
