# ProvisioningManager Module

This lab (`labs/ProvisioningManager`) documents a self-contained Wi-Fi provisioning helper for ESP32-class boards. Implementation files now live under `apps/ProvisioningManager/` so they can be pulled directly into your main control app. The original `labs/APwifi` sketch remains untouched for reference.

## Files
- `apps/ProvisioningManager/ProvisioningManager.h/.cpp` — reusable provisioning component (no `.ino`, ready for integration).
- `ProvisioningCheatSheet.md` — quick reference for configuration, HTTP endpoints, serial commands, and UDP events.
- `examples/ProvisioningTest/ProvisioningTest.ino` — serial-friendly demo to exercise the portal.

## Using the manager in your sketch
1. Include the header: `#include "ProvisioningManager.h"` (adjust path if used from another folder, e.g., from a sketch under `apps/...` use `#include "../ProvisioningManager/ProvisioningManager.h"`).
2. Fill out a `ProvisioningConfig` with your AP SSID, password, LED pin, and timing preferences.
3. Instantiate `ProvisioningManager mgr(cfg);`.
4. Call `mgr.begin();` in `setup()` and `mgr.loop();` from `loop()`.

> **Note:** When including from another sketch, also `#include "../ProvisioningManager/ProvisioningManager.cpp"` (or equivalent path) so the Arduino builder compiles the implementation. For production, copy `ProvisioningManager.{h,cpp}` into that sketch folder (or create a proper library) and drop the explicit `.cpp` include.

The manager automatically:
- Hosts a captive portal web app (`/`, `/scan`, `/save`, `/status`).
- Persists credentials in NVS (`Preferences` namespace `wifi_config`).
- Connects to the requested network and tears down the AP once successful.
- Broadcasts a `provisioning_complete` UDP packet (port configurable) for companion apps.
- Handles a `reset` serial command to clear credentials and relaunch provisioning.

## Testing flow
1. Flash `examples/ProvisioningTest/ProvisioningTest.ino`.
2. Open the Serial Monitor (115200 baud) and follow the printed instructions.
3. Join the AP SSID shown in the config, browse to `http://192.168.4.1`, choose a network, and submit credentials.
4. Watch for the UDP broadcast on your laptop/phone to confirm completion (see cheat sheet).
5. Type `reset` in the Serial Monitor to return to provisioning mode.

For API details and integration pointers see `ProvisioningCheatSheet.md`.
