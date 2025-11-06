# W25Q128 Flash Store

Utility class intended to front a Winbond W25Q128 (or similar) SPI flash
device for simple record storage. The current scaffold provides:

- `begin()` for wiring the chip select, clock, and data lines.
- `writeRecord()` to store a byte payload.
- `readLatest()` to retrieve the newest payload.
- `getStats()` exposes aggregate usage/health metrics derived from the wear
  estimator used in the mini Flash Database project.
- `simulateUsage()` lets higher-level code seed sector/erase counts when the
  real FlashLogger is the owner of that data.

Include it with:

```cpp
#include "modules/flash/include/FlashStore.h"

FlashStore flash;
flash.begin();
flash.simulateUsage(/*used*/ 512, /*totalEraseOps*/ 4000, /*avgBytesPerDay*/ 128 * 1024.0f);
auto stats = flash.getStats();
```

## Supporting files

- `examples/basic_usage/basic_usage.ino` — demonstrates writing/reading a record.
- `checkers/flash_checker.ino` — quick loop performing a pattern self-test.
- `tests/flash_test.cpp` — host assertions (`g++ -Imodules/test_support -Imodules/flash/include modules/flash/tests/flash_test.cpp modules/flash/src/FlashStore.cpp -o flash_test`).

> TODO: Back the stubbed methods with actual SPI transactions and consider
> wear levelling for long-term logging.
