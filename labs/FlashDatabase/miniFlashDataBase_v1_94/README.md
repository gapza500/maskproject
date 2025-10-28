# miniFlashDataBase v1.94

API clean-up pass: configuration struct, richer shell helpers, and factory
metadata refresh.

## Use

1. Build & upload `tests/config_shell_demo_v1_94.ino` (defaults target the Cytron
   Maker Feather AIoT S3 + DS3231 on SDA=42/SCL=41; tweak pins in the sketch for
   other boards).
2. Serial @115200 should emit:
   - `[cfg] begin(cfg) -> OK` confirming the new `FlashLoggerConfig` path.
   - `[factory] reset` block wiping sectors and re-initialising indexes.
   - `[append]` lines for a handful of demo payloads.
   - Shell-driven sections for `ls`, `stats`, `pf`, and `factory`.
   - `FlashStats` line derived from `getFlashStats(...)`.

If you rerun the sketch, the factory info persists and the listings should grow
with the new samples.

## Notes

- `FlashLoggerConfig` centralises pin/date/output defaults so production code
  can share a single struct between boot, CLI, and upload helpers.
- `handleCommand(...)` now routes `reset`, `ls`, `cd`, `stats`, `pf`, `factory`,
  and the cursor/export verbs; the demo drives them end-to-end.
- `factoryReset(code)` keeps the identity block (`model/flash/device id`) and
  re-seeds the active day/sector via `reinitAfterFactoryReset()`.
- `getFlashStats(avgBytesPerDay)` exposes health numbers for dashboards or
  telemetry; the sketch prints an example summary.
