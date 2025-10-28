# Release Notes

Summary of notable changes from v1.0 through v2.0.

## v1.0 → v1.8

- Core append-only logger with commit markers and CRC protection.
- RTC integration (DS3231) with Thai/ISO/US date styles.
- Crash-safe writes and grouped per-day printing.
- Simple wear tracking and factory info storage.

## v1.9 (Shell)

- Interactive serial shell (`ls`, `cd`, `print`, `info`, `stats`, `factory`).
- Navigation caches for days/sectors.

## v1.91 – v1.92 (Queries)

- Summaries cached for faster navigation.
- Query engine with CSV/JSONL output and field filtering.
- `q latest`, `q day`, `q range`, `fmt`, `set csv` shell commands.

## v1.93 (Sync Cursors)

- `SyncCursor` struct with `getCursor`, `setCursor`, `exportSince`.
- Shell support for cursor show/clear/set/export.

## v1.94 (Config API)

- `FlashLoggerConfig` struct for parametric initialisation.
- `begin(cfg)` refactor and config persistence groundwork.

## v1.95 (Anchors + Diagnostics)

- Persisted cursor to NVS (`saveCursorNVS`, `loadCursorNVS`).
- Anchor index per sector for faster range scans.
- `scanbad` command and consolidated `rescanAndRefresh` helper.

## v1.96 (Pagination)

- Page tokens for queries/export.
- Reverse scan path for `queryLatest`.
- Auto-save export cursor after successful streaming.

## v1.97 (Transactional GC + RTC Guard)

- Erase-intent headers for power-loss-safe GC.
- RTC fallback storing last good timestamp in NVS.
- `help` command with full command list.

## v1.98 (Config Persistence + Upload Helpers)

- `saveConfigToNVS` / `loadConfigFromNVS` snapshot runtime settings.
- Upload helpers for NDJSON/CSV with retry/backoff/idempotency.

## v1.99 (Predicates + Battery Guard)

- Predicate filters (`FieldPredicate`) for query/export.
- Persisted anchors rebuilt lazily.
- Battery guard hooks to pause logging at low SOC.

## v2.0 (Release)

- Documentation & example suites consolidated.
- Regression harnesses for pagination and predicate exports.
- Ready for packaging (`library.properties`, metadata) and publish.

EOF
