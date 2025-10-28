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
