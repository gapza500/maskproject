# miniFlashDataBase v1.98

Targets for this milestone:

- [x] **Persist FlashLoggerConfig** – snapshot runtime config to NVS and allow
  reloading overrides at boot (`FlashLogger::saveConfigToNVS`, `loadConfigFromNVS`).
- [x] **Upload helpers** – provide thin NDJSON/CSV streaming helpers with
  retry/backoff and idempotency keyed by record sequence (`UploadHelpers.*`).
- **Housekeeping** – document the upload APIs and provide example integration.

## Use

1. Flash `tests/upload_config_demo_v1_98.ino` (defaults to the Cytron Maker
   Feather AIoT S3 wiring; adjust the `board` namespace for other targets).
2. Open Serial @115200. A successful run prints:
   - `[config] loaded from NVS …` showing the persisted overrides.
   - An `ndjson` section where the first send attempt is intentionally failed
     and retried with exponential backoff before succeeding.
   - A `csv` section streaming the same records with idempotency keys.
   - Final `[test] complete` line.

If the RTC module is missing, the harness will halt after the warning until the
DS3231 is restored.

## Notes

- `FlashLoggerConfig.persistConfig=true` plus `configNamespace` opt-in to NVS
  storage. Reboots can call `loadConfigFromNVS` to restore pin/date/output
  overrides before spinning up the logger.
- Upload helpers wrap `exportSinceWithMeta`, formatting payloads as NDJSON or
  CSV while invoking a user-supplied sender with an idempotency key derived from
  the record sequence.
- The retry policy accepts `maxAttempts`, `initialBackoffMs`, and
  `backoffMultiplier`; the test harness injects a single failure to demonstrate
  the behaviour.
