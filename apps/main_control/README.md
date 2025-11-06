# Main Control Sketch

`main_control.ino` orchestrates the device status providers inside a
production-style loop:

- polls device health for 3 minutes, then enters a 12-minute deep sleep cycle
- keeps the SSD1309 dashboard up-to-date through the shared `screen_manager`
  backend and the `OledStatusProvider`
- hands Wi-Fi onboarding to `ProvisioningManager`; once credentials are saved
  it synchronises the DS3231 RTC via NTP and shares the time via
  `device_status::GlobalTime`
- exposes aggregated metrics (SEN66 readings, flash usage/health, RTC status)
  on both the OLED and Serial console
- appends each measurement to the W25Q flash via `FlashLogger`, so cold boots
  resume logging without losing history
- short press of the front button (GPIO 15) cycles dashboard views; hold for 5 s
  to wipe credentials/logs and reboot into provisioning mode

## Configuration

Provisioning has been disabled in this offline build—the sketch skips the captive
portal entirely and runs with Wi-Fi disabled. `sys provisioned` simply toggles the stored flag
but does not spawn the AP. NTP settings live
in `include/time_sync.h` (timezone/interval constants).

## Sleep Behaviour

The loop mirrors the duty-cycle used in the labs: 180 seconds of active
refreshing followed by 720 seconds of sleep. Adjust the cadence (active,
sleep, measurement, summary, sync windows) via `include/runtime_config.h`
if the production timing needs to change.

## Next Steps

- Integrate application-specific logic (MQTT, data logging, etc.) inside the
  active window before the device drops back to sleep.
- Once the MAX17048 hardware is repaired, add its provider back into the
  `DeviceStatusManager` so battery state joins the aggregated report.
- Swap the cloud/local/BLE comms stubs for real endpoints and hook the
  FlashLogger cursor into your sync pipeline.

## Logging smoke-test

After flashing the sketch:

1. Let the unit run through at least one measurement cycle (3 minutes).
2. Trigger `flashLogger.printFormattedLogs()` from the Serial shell (or add a
   temporary call) to confirm JSON records are landing in the W25Q flash.
3. Inspect the OLED/Serial “Flash” stats to see them reflect the written data.

### Serial diagnostics

- Structured logs now appear on the serial monitor in the form
  `[000012345][INFO][measure] …`, making it easier to trace state transitions,
  sensor reads, and warnings during bring-up.
- Type `help` in the serial console to see the combined dashboard + miniFlash
  shell commands. FlashLogger commands (`flash help`, `flash ls`, `flash q latest`,
  `flash export`, `flash cursor …`, etc.) are proxied directly, so you can
  inspect or export logs without rebuilding the firmware. The UI now auto-cycles
  between the particulate “graph” view (30 s) and the indoor comfort “feeling”
  view (10 s) while measuring; any manual screen change disables the auto-loop
  until the next wake cycle. Use `alert ack [min]` to temporarily silence
  connectivity alerts during testing, `alert resume` to re-enable them, and `sys provisioned`
  to skip provisioning (offline mode). During the original captive-portal flow, holding
  the front button for 5 seconds also skipped provisioning, but the current build boots
  directly into offline mode by default.

## API smoke-tests

The communications headers are stubbed but already expose basic entry points you
can exercise today:

- **Cloud push:** `comms::cloud::publishIfDue(...)` currently logs a
  "[cloud] publish stub" line to Serial whenever the Wi-Fi link is up and the
  publish interval elapses. Watch the console to confirm cadence before wiring
  a real MQTT/REST client.
- **Local Wi-Fi API:** the `comms::local_api::Service` hosts an HTTP server with
  a `/status` route. After provisioning, open
  `http://<device-ip>:8080/status` in a browser or run
  `curl http://<device-ip>:8080/status` to see the JSON summary.
- **BLE/GATT:** `comms::ble::Transport` advertises the device using NimBLE and
  notifies a placeholder characteristic (`180A/2A57`). Use a BLE scanner on your
  phone (nRF Connect, LightBlue, etc.) to confirm the service appears; the
  payload will show SEN66/flash flags until you define the final schema.
