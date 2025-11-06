# Device Status Library

This lab bundles a reusable set of helpers for aggregating the health of the
air-monitor firmware peripherals:

- **SEN66** air-quality sensor
- **MAX17048** battery fuel gauge
- **DS3231** RTC / backup battery
- **SSD1309** OLED
- **W25Q128** external flash

## Layout

```
labs/devicestatus_lib/
├── device_status.h                   // Aggregator include for sketches
├── include/device_status/            // Public headers
│   ├── DeviceStatusCodes.h           // shared enums + status labels
│   ├── DeviceStatusManager.h         // orchestrator for providers
│   ├── Report.h                      // aggregated status payload
│   ├── StatusProvider.h              // base interface
│   └── providers/                    // concrete adapters
├── src/                              // Implementation (.cpp) files
└── devicestatus_lib.ino              // usage example / smoke sketch
```

Your sketch only needs to `#include "device_status.h"` and then instantiate the
providers you care about. Each provider updates a shared
`DeviceStatusReport`, allowing the main app to pull a ready-to-use snapshot of
all device health and key telemetry (battery %, SEN66 measurements, RTC flags,
etc.).

## Typical wiring / setup

```cpp
#define SCREEN_BACKEND_U8G2
#include <U8g2lib.h>

#include "modules/screen_manager/include/screen_config.h"
#include "modules/screen_manager/include/screen_manager.h"
#include "modules/ds3231/include/Ds3231Clock.h"
#include "device_status/GlobalTime.h"

#include "device_status.h"
#include "device_status/providers/Sen66StatusProvider.h"
// ...include other providers you need...

SensirionI2cSen66 sen;
Max17048 gauge;
Ds3231Clock rtc;
screen_manager::ScreenManager screen;
U8G2_SSD1309_128X64_NONAME0_F_SW_I2C oled(U8G2_R0, /*clock=*/22, /*data=*/21, U8X8_PIN_NONE);
FlashStore flash;

device_status::DeviceStatusReport report;
device_status::DeviceStatusManager manager(report);

device_status::Sen66StatusProvider senProvider(sen, Wire);
device_status::OledStatusProvider oledProvider(screen);
device_status::Ds3231StatusProvider rtcProvider(rtc);
// Add the rest...

void setup() {
  Wire.begin();              // configure I2C pins before beginAll()
  oled.begin();              // power up SSD1309 (via U8g2 in this example)
  screen.begin(oled);        // bind screen manager to the backend
  manager.addProvider(senProvider);
  manager.addProvider(oledProvider);
  manager.addProvider(rtcProvider);
  // manager.addProvider(...);
  manager.beginAll();        // init each provider once
}

void loop() {
  manager.refreshAll();      // refresh cached report inside active window
  // [do work for ~3 minutes...]
  // then put MCU into deep sleep for 12 minutes with esp_sleep API (see example sketch)
}
```

Provider-specific notes:

- **SEN66**: runs continuous measurement and caches particulate data, humidity,
  temperature, VOC/NOx indices, and the raw device-status word. Error bits map
to `STATUS_WARN`/`STATUS_ERROR` automatically.
- **MAX17048**: exposes percent/voltage readings and flags WARN/ERROR when the
  pack drops below configurable soft/critical thresholds.
- **DS3231**: wraps RTClib's `RTC_DS3231`, pushes the current `DateTime` into
  `device_status::GlobalTime` for app-wide access, and flags WARN/ERROR when the
  backup battery or oscillator misbehaves.
- **OLED**: reuses the shared `screen_manager::ScreenManager` renderer. Provide
  an already-initialised screen manager (tied to U8g2/Adafruit backends) and the
  provider will keep headline status text refreshed automatically.
- **W25Q128**: can run a guarded read/write self-test on demand. Disable the
  self-test if the flash stores production data you cannot overwrite.

## Integration checklist

1. Inject your project-specific `TwoWire`/`SPI` instances into the providers so
   the bus configuration stays centralised in the main app.
2. Decide on refresh cadence. For production builds poll inside an RTOS task or
   timer rather than the tight loop shown in the example sketch.
3. Extend `DeviceStatusReport` if the main application needs more metrics
   (e.g. flash wear counters, OLED frame rate). Keep the struct zero-cost by
   preferring POD members.
4. Surface aggregated status upstream – e.g. push to BLE packets, MQTT, or the
   OLED layout – instead of letting each provider talk to the UI.
5. Run the host-side smoke test in `labs/devicestatus_lib/tests/status_manager_test.cpp`
   (`g++ -Itests -Iinclude tests/status_manager_test.cpp src/DeviceStatusManager.cpp -o status_test`)
   to validate the manager + report plumbing before flashing to hardware. Add
   device-specific mocks there as real drivers land.

6. Add `@testing-library`/Playwright checks once the Next.js front-end consumes
   these status flags so regressions are caught automatically.

## Testing

The example sketch in this folder (`devicestatus_lib.ino`) exercises all
providers on hardware. For CI, you can create fake drivers implementing the same
interfaces and run host-side unit tests to confirm state transitions (e.g.
critical battery threshold, SEN66 error bits, W25Q128 self-test failure).

## Global time access

`device_status::GlobalTime` caches the latest `DateTime` observed by the
DS3231 provider. Call `GlobalTime::current()` or `GlobalTime::unix()` anywhere
in the app to retrieve the most recent synchronized timestamp. When the device
later syncs with Wi-Fi/NTP, update the RTC via `Ds3231Clock::adjust()` and the
GlobalTime cache will follow automatically.
