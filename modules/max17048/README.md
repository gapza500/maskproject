# Max17048 Fuel Gauge Helper

Simple helper for the MAX17048/MAX17049 I2C fuel gauge. Provides:

- `begin()` to probe the device.
- `readPercent()` returning state-of-charge in percent.
- `readVoltage()` returning battery voltage in volts.

Usage:

```cpp
#include "modules/max17048/Max17048.h"

Max17048 fuelGauge;

fuelGauge.begin();
float soc = fuelGauge.readPercent();
```

## Supporting files

- `examples/basic_usage/basic_usage.ino` — drop-in sketch streaming SOC/voltage.
- `checkers/max17048_checker.ino` — diagnostic loop verifying gauge responses.
- `tests/max17048_test.cpp` — host sanity test (`g++ -Imodules/test_support -Imodules/max17048 modules/max17048/tests/max17048_test.cpp -o max17048_test`).
