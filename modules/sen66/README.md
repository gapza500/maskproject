# Sensirion SEN66 Helper

Thin wrapper around a Sensirion SEN66 particulate and environmental sensor.
The current stub exposes:

- `begin()` to initialise the bus (replace with real driver setup).
- `readOnce()` to fetch one set of readings.
- Accessors `pm25()`, `temperature()`, and `humidity()` for the cached data.

Include it with:

```cpp
#include "modules/sen66/include/Sen66Driver.h"
```

## Supporting files

- `examples/basic_usage/basic_usage.ino` — quick start sketch printing readings.
- `checkers/sen66_checker.ino` — serial diagnostic loop to validate hardware.
- `tests/sen66_test.cpp` — host test to keep the stub interface wired up
  (`g++ -Imodules/test_support -Imodules/sen66/include modules/sen66/tests/sen66_test.cpp modules/sen66/src/Sen66Driver.cpp -o sen66_test`).

> TODO: Swap the placeholder implementation with real Sensirion SEN driver
> calls and expose continuous sampling helpers as needed.
