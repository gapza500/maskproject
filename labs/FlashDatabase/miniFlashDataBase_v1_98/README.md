# miniFlashDataBase v1.98

Targets for this milestone:

- [x] **Persist FlashLoggerConfig** – snapshot runtime config to NVS and allow
  reloading overrides at boot (`FlashLogger::saveConfigToNVS`, `loadConfigFromNVS`).
- [x] **Upload helpers** – provide thin NDJSON/CSV streaming helpers with
  retry/backoff and idempotency keyed by record sequence (`UploadHelpers.*`).
- **Housekeeping** – document the upload APIs and provide example integration.

Supporting work:

1. Ensure pagination tokens/export continue working with persisted config.
2. Add tests/mocks for the retry/backoff path.
3. Update README once example integrations are ready (see `examples/`).
