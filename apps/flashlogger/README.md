# miniFlashDataBase Integration Bundle

This folder collects the production-ready sources required by the main app.

## Contents

- `src/FlashLogger.{h,cpp}` – append-only flash logger with predicates and GC.
- `src/UploadHelpers.{h,cpp}` – NDJSON/CSV upload helpers with retry/backoff.

Refer to `labs/FlashDatabase/miniFlashDataBase_v2_0/docs/` for detailed
documentation (getting started, API reference, storage model, sync guide, etc.).

## Quick Usage

```cpp
#include "FlashLogger.h"
#include "UploadHelpers.h"

FlashLogger logger;
FlashLoggerConfig cfg;
// configure pins, RTC, etc.
logger.begin(cfg);
```

Persist cursors/config using `saveCursorNVS` / `loadCursorNVS` and drive uploads
with `flashlogger_upload_ndjson` or `flashlogger_upload_csv`.

Test harnesses live under:
- `labs/FlashDatabase/miniFlashDataBase_v1_96_tests/`
- `labs/FlashDatabase/miniFlashDataBase_v1_99_tests/`


## Example

- `examples/airmonitor_sync.ino` – mock AirMonitor loop combining logging and upload helper.
