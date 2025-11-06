
# IoT Monorepo (ESP32 DevKit v1)

This repo hosts the main control sketch under `apps/`, shared modules under `modules/`, and experimental spikes in `labs/`. Everything builds with the Arduino IDE or `arduino-cli`.

## Key folders
- `apps/main_control/` — current firmware (state machine, provisioning portal, comms)
- `apps/ssd1309_dashboard/` — reference OLED dashboard sketch (matches the screen layout used in `main_control`)
- `modules/` — reusable components (`sen66`, `flash`, `screen_manager`, `ds3231`, board pin maps, etc.)
- `labs/` — hardware experiments and diagnostics (ProvisioningManager, miniFlash database, sensor checkers)
- `libraries/` — vendored Arduino dependencies

## Current work plan
1. **Provisioning & Wi-Fi UX**
   - AP runs in `WIFI_AP_STA` mode so `/scan` stays available while the phone is connected.
   - `/scan` returns live SSID/RSSI/auth lists; `/save` stores credentials in NVS and reboots into STA mode.
2. **Device status stack**
   - `DeviceStatusManager` polls SEN66, DS3231, and Flash providers each measurement loop.
   - Serial monitor commands (`help`, `status`, `sample`, `button`) refresh the providers on demand and log detailed output (look for the `[sen66]…` lines).
3. **Screen/dashboard**
   - Single button short-press cycles pages; long-press performs the factory reset.
   - Dashboard trend graph auto-scales even when readings are flat, so PM history is always visible.
4. **miniFlash integration**
   - Flash logger initialises at boot and exposes its shell commands (`ls`, `q latest`, `export`, `cursor …`) via the same serial console.

## System architecture plan

### 1. Main control & state machine
- States: **Init** (self-check, NVS flag), **Measure** (data acquisition), **Sync** (cloud/BLE upload), **Sleep** (deep sleep with watchdog).
- Init decides between provisioning vs. resuming saved state (flash cursor, user prefs).
- Sleep duration currently 3 minutes active / 12 minutes sleep (tune timing knobs in `apps/main_control/include/runtime_config.h`).
- Provisioning portal SSID/password, IP block, scan cadence, and AP/STA behaviour are also configurable via `apps/main_control/include/runtime_config.h` so you can match field hardware quirks.

### 2. Provisioning algorithm
- ESP32 hosts an AP (`Device-Setup`), renders a captive web portal, and accepts SSID/password.
- Credentials persist in NVS; on success the device reboots into STA mode and the dashboard shows the last provisioned SSID.
- `/scan` provides RSSI + auth info without dropping the AP thanks to AP/STA mode.

### 3. RTC management
- DS3231 is the single source of timestamps for logs, dashboard clock, and sync cursors.
- NTP sync runs during the Sync state (or via a future Wi-Fi-on-demand trigger).
- Battery-low flag (DS3231) surfaces through `screen_control::setWarning` and flash health summaries.

### 4. Data acquisition
- SEN66 polls for readiness (`getDataReady`) before fetching particulates, VOC, NOx, temperature, humidity, CO2.
- Measurement packs sensor data plus battery percent/voltage and DS3231 timestamp.
- Sensor gets powered down between cycles to reduce heat/power (TODO: add GPIO control hook).

### 5. Data logging
- Flash logger writes JSON NDJSON records to W25Q128 in a circular buffer.
- NVS stores the write cursor so logging resumes after resets.
- Records tagged as “sent” stay on flash for 7+ days for offline recovery.

### 6. Data synchronization
- Wi-Fi is first tier (HTTP batches); BLE transport is second tier for phone pull.
- Retry mechanics should send backlog in manageable chunks with cursors (miniFlash handles pagination tokens).
- Future work: MQTT/cloud selection, BLE GATT schema matching the flash payload.

### 7. Display & user interface
- Screen manager renders status bar (Wi-Fi, Bluetooth, alerts), dashboard trend, and detail pages.
- Alerts highlight low battery, high PM, connectivity loss, or RTC faults.
- Emoji dashboard mirrors the `modules/screen_manager` demo.

### 8. Control button algorithm
- **Short press (<5 s):** cycle screen pages (PM summary → gases → comfort → device → user).
- **Long press (≥5 s):** factory reset (clears credentials, flash cursor, reboots to provisioning).
- Serial shortcut `button` mimics the short press for bench testing.

### 9. Battery management
- MAX17048 reports SoC and voltage; data is logged with every record and shown on the dashboard.
- Battery health heuristics (GOOD/FAIR/LOW) guide screen alerts and sync throttling.
- Future enhancement: calibration curve based on ADC fallback if the gauge is absent.

## Quick start
1. Open `apps/main_control/main_control.ino` in the Arduino IDE.
2. Copy any required secrets templates (e.g. Wi-Fi credentials) into `apps/main_control/include/`.
3. Select **ESP32 Dev Module** (DevKit v1), build, and upload.
4. After boot, connect to the provisioning AP (`Device-Setup` / `password123`) or type `help` in the serial monitor to explore the dashboard/flash commands.

## CI
CI compiles all sketches (apps + labs) via `arduino-cli`. Keep library versions pinned before merging.
