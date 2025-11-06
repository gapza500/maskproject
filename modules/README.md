# Modules Directory

This folder groups reusable hardware modules and board profiles. Each board
profile now keeps its pin assignment as both:

- a table in `README.md` for quick reference, and
- a C++ header that exposes the pins via a dedicated namespace.

Include the header that matches your target board, for example:

```cpp
#include "../../../modules/cytron_maker_feather_aiot_s3/pins_cytron_maker_feather_aiot_s3.h"
#include "../../../modules/max17048/Max17048.h"

using namespace board_pins::cytron_maker_feather_aiot_s3;

Max17048 fuel;
if (fuel.begin()) {
  float soc = fuel.readPercent();
}
```

Each sensor/peripheral module now follows a consistent layout:

- `include/` and `src/` for the lightweight C++ wrapper.
- `examples/` for ready-to-flash demonstration sketches.
- `checkers/` for quick hardware diagnostics you can run on the bench.
- `tests/` for host-built assertions using the stubbed `modules/test_support/Arduino.h`
  header (`g++ -Imodules/test_support -Imodules/<module>/include ...`).

When adding a new hardware revision or module, mirror this structure: document
the wiring in `README.md`, provide headers with clear APIs, and seed examples,
checkers, and tests so every device can be exercised in isolation before it is
pulled into the larger app.

> Heads up: keep directory names filename-safe (lowercase + underscores) so the
> Arduino build system can resolve the `#include` paths without issue. Legacy
> folders with spaces are retained for reference, but new code should include
> the sanitized paths. Adjust the relative prefix (`../../../`) to match the
> structure of your sketch folder.

## Available Modules

- `max17048` — I2C fuel-gauge helper exposing state-of-charge and battery voltage.
- `sen66` — Sensirion SEN66 particulate/temperature/humidity stub ready to wire to the official driver.
- `flash` — Placeholder W25Q128 SPI flash store with hooks for record logging.
- `ds3231` — Notes and integration guidance for the DS3231 real-time clock.
