# Screen Manager Module

This module provides a reusable rendering pipeline for SSD1309 (128×64) OLED dashboards. The code is split into small headers so sketches and larger “main control” projects can stay minimal while managing complex layouts.

## Layout and configuration

- `screen_config.h` – central location for dimensions, timing, and enums. Dashboard-specific constants (page interval, sample count, graph offsets) are defined here.
- `apps/ssd1309_dashboard/include/pm_emoji_bitmaps.h` – PROGMEM emoji faces used by the dashboard overlay.

## Rendering helpers

- `apps/ssd1309_dashboard/include/dashboard_view.h` – encapsulates the OLED overlay (PM trend graph + detail page). Feed it with `dashboard_view::DashboardData` and attach it to a `screen_manager::ScreenManager` via `begin()`.
- `apps/ssd1309_dashboard/include/screen_control.h` – owns display state through `screen_control::ControlContext`. Exposes helpers to initialise hardware, update data (PM, temperature, humidity, etc.), switch screens, apply status icons, and tick the renderer. Runtime logic is implemented in `apps/ssd1309_dashboard/src/screen_control.cpp`.

## Serial interface

- `apps/ssd1309_dashboard/include/screen_serial.h` – command router that converts strings such as `pm 12.3`, `temp 27`, or `screen 3` into the appropriate `screen_control` calls. Implementation lives in `apps/ssd1309_dashboard/src/screen_serial.cpp`. Use `screen_serial::printHelp()` to list the available commands.

## Example sketch

`apps/ssd1309_dashboard/ssd1309_dashboard.ino` shows how to wire everything together. The sketch is intentionally tiny—the heavy lifting is in the headers—so you can reuse the structure in your main control firmware:

```cpp
screen_control::ControlContext ctx;
screen_manager::ScreenManager screen;
dashboard_view::DashboardView dashboard;

void setup() {
  screen_control::initialize(ctx, screen, dashboard, rtc,
                             flashStats, factoryInfo,
                             screen_control::ScreenId::Dashboard, millis());
  screen_serial::printHelp();
}

void loop() {
  screen_control::updateDashboard(ctx, millis());
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    screen_serial::handleCommand(ctx, cmd, millis());
  }
  screen_control::tick(ctx, millis());
}
```

Replace the serial commands with your own sensor updates by calling the corresponding setters declared in `apps/ssd1309_dashboard/include/screen_control.h` (e.g. `setPm25`, `setTemperature`, `setWarning`).

## Serial command reference

The example uses `screen_serial::handleCommand()` to interpret console input. You can keep this for debugging, feed the commands from a Bluetooth terminal, or map them to buttons in your main control project. Supported commands:

| Command | Action |
|---------|--------|
| `screen next` / `screen prev` | Cycle between dashboard, particulate, gases, comfort, device, and user info pages |
| `screen <1-6>` | Jump to a specific page |
| `dash summary` / `dash detail` | Force dashboard view to the graph (summary) or emoji detail page |
| `pm <ug/m3>` | Update `pm25` reading (recomputes derived PM1/PM4/PM10 values) |
| `temp <celsius>` | Update temperature |
| `hum <percent>` | Update humidity |
| `co2 <ppm>` / `voc <ppm>` / `nox <ppm>` | Update gas readings |
| `battery <percent>` | Update battery state (status bar + page data) |
| `wifi on|off`, `bt on|off`, `link <0-3>` | Adjust status bar indicators |
| `warn on|off` | Toggle a sticky warning alert |
| `connect` | Stamp the “last connection” timer |
| `ssid <text>`, `user <text>`, `model <text>` | Update the metadata displayed on the user page |
| `help` | Reprint the command list |

If you prefer physical buttons, call the same helper functions from your event handlers (e.g. `screen_control::setActiveScreen(ctx, nextScreen(ctx.activeScreen), now)`).

## Integration checklist for main control

1. **Include the headers** – add `#include "apps/ssd1309_dashboard/include/screen_control.h"`, `apps/ssd1309_dashboard/include/dashboard_view.h`, and optionally `apps/ssd1309_dashboard/include/screen_serial.h` to your project.
2. **Create shared objects** – instantiate `screen_manager::ScreenManager`, `dashboard_view::DashboardView`, `Ds3231Clock` (or your RTC), `FlashStats`, and a `screen_control::ControlContext`.
3. **Hardware setup** – initialise I²C, RTC, and any flash diagnostics before calling `screen_control::initialize()`.
4. **Initialise the controller** – pass the objects to `screen_control::initialize(ctx, screen, dashboard, rtc, flashStats, factoryInfo, ScreenId::Dashboard, millis());`
5. **Feed data** – whenever a sensor reading changes, call the appropriate setter (`setPm25`, `setTemperature`, `setHumidity`, `setCo2`, `setVoc`, `setNox`, etc.).
6. **Update metadata** – use `setSsid`, `setUser`, `setModel`, and `recordConnection` when network/user state changes.
7. **Status/alerts** – toggle `setWifi`, `setBluetooth`, `setLinkBars`, `setBatteryPercent`, and `setWarning` from your connectivity and power-management code.
8. **Main loop** – call `screen_control::updateDashboard(ctx, millis());` at your preferred cadence (the helper already throttles to 1 Hz) and always call `screen_control::tick(ctx, millis());` once per loop to drive dirty-region rendering and auto alerts.

By following the checklist above, you can drop the example `.ino` entirely and embed the same screen behaviour inside your main control application.
