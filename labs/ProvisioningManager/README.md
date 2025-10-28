# ProvisioningManager Module

This lab (`labs/ProvisioningManager`) contains a self-contained Wi-Fi provisioning helper for ESP32-class boards. It spins up a captive portal, saves credentials to NVS, and transitions to STA mode while broadcasting a completion event that a companion mobile app can consume. The original `labs/APwifi` sketch remains untouched for reference.

## Files
- `ProvisioningManager.h/.cpp` — reusable provisioning component.
- `ProvisioningCheatSheet.md` — quick reference for configuration, HTTP endpoints, serial commands, and UDP events.
- Demo sketch lives under `apps/ProvisioningManagerDemo/ProvisioningManagerDemo.ino` so it is easy to upload as a main app.

## Using the manager in your sketch
1. Include the header: `#include "ProvisioningManager.h"` (adjust path if used from another folder, e.g., the demo uses `../../labs/ProvisioningManager/ProvisioningManager.h`).
2. Fill out a `ProvisioningConfig` with your AP SSID, password, LED pin, and timing preferences.
3. Instantiate `ProvisioningManager mgr(cfg);`.
4. Call `mgr.begin();` in `setup()` and `mgr.loop();` from `loop()`.

> **Note:** The demo sketch includes `ProvisioningManager.cpp` directly so the Arduino builder compiles the implementation. In production code move `ProvisioningManager.{h,cpp}` into your sketch folder (or into `libraries/`) so the `.cpp` is built automatically.

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
