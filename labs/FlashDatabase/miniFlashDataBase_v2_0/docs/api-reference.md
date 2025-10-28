# API Reference

This section documents the primary classes, structs, and functions exported by
miniFlashDataBase v2.0. See inline comments in `FlashLogger.h` for full detail.

## FlashLogger

### Construction

```cpp
FlashLogger logger;              // default, configure via begin(config)
FlashLogger logger(csPin);       // legacy constructor for quick sketches
```

### Initialization

```cpp
FlashLoggerConfig cfg;
// Populate fields (see table below), then:
logger.begin(cfg);
```

Key `FlashLoggerConfig` fields:

| Field | Default | Notes |
| --- | --- | --- |
| `rtc` | `nullptr` | Required `RTC_DS3231*`. Library aborts if missing. |
| `spi_cs_pin` | 4 | CS pin used internally by FlashLogger. |
| `spi_sck_pin` / `spi_mosi_pin` / `spi_miso_pin` | `-1` | Provide explicit pins when not using default VSPI/FSPI. |
| `spi_clock_hz` | 20 MHz | Lower this for long wires or when using test rigs. |
| `totalSizeBytes` | 8 MB | Total flash size. Set to 16 MB for W25Q128. |
| `sectorSize` | 4096 bytes | Leave at 4 KB for Winbond parts. |
| `retentionDays` | 7 | Used by GC (`gc()`) to decide when to reclaim pushed days. |
| `defaultOut` | `OUT_JSONL` | Initial output format for shell queries. |
| `csvColumns` | `"ts,bat,temp"` | Default columns when using CSV output. |
| `persistConfig` | `false` | Persist config snapshot to NVS (`saveConfigToNVS`). |
| `configNamespace` | `"flcfg"` | Namespace used when `persistConfig` is true. |
| `enableShell` | `true` | Toggle built-in serial shell. |

### Writing

```cpp
logger.append("{\"temp\":25.3,\"bat\":90}");
```
Records are newline-terminated JSON payloads. Append returns `false` if the
flash is full, the RTC guard trips, or the battery guard is active.

### Queries

```cpp
QuerySpec q;
q.out = OUT_JSONL;
q.predicates[0] = {"bat", PRED_LE, 20.0f};
q.predicateCount = 1;
logger.queryLogs(q, onRowCallback, userPointer);
```

Available helpers:

- `queryLogs` – generic time/predicate constrained query.
- `queryLatest(N, ...)` – newest `N` records with optional pagination token.
- `exportSince(cursor, maxRows, onRow, user, nextToken)` – stream from a cursor
  with automatic pagination token.
- `exportSinceWithMeta(cursor, maxRows, onRecord, user, filter, nextToken)` –
  exports with access to headers (sequence, timestamp) and predicate filters.

### Cursors

```cpp
SyncCursor cur;
logger.getCursor(cur);
logger.setCursor(cur);
logger.clearCursor();
logger.saveCursorNVS("flog", "cursor");
logger.loadCursorNVS("flog", "cursor");
```

Use cursors to build resumable uploads. The serial shell exposes
`cursor show|clear|set|save|load` and `export` commands that map to these APIs.

### Maintenance

- `markDayPushed(dayID)` / `markDaysPushedUntil(dayID)` – mark days safe for GC.
- `gc()` – transactional garbage collection; skips sectors not marked `pushed`.
- `rescanAndRefresh(rebuildSummaries, keepSelection)` – rebuild indexes and
  lazy anchors after resets or power loss.
- `factoryReset(code12)` – wipes all data sectors while preserving factory info.

### Shell Commands

`handleCommand(String, Stream&)` supports:

```
help, ls, ls sectors, cd day, cd sector, print, info,
q latest/day/range, fmt, set csv,
cursor show/clear/set/save/load,
export, stats, factory, gc, scanbad, reset <code>
```

## Upload Helpers

`UploadHelpers.h` provides NDJSON/CSV wrappers around
`exportSinceWithMeta` with retry/backoff and idempotency keys:

```cpp
FlashLoggerUploadPolicy policy;
policy.maxAttempts = 3;
policy.initialBackoffMs = 200;
policy.backoffMultiplier = 2.0f;

flashlogger_upload_ndjson(logger, cursor, 128, senderCallback, user, policy, &nextToken);
```

The sender callback signature:

```cpp
bool sender(const char* payload, size_t len,
            const char* idempotencyKey, void* user);
```

Return `false` to trigger a retry (up to `maxAttempts`). The CSV variant mirrors
NDJSON but uses the logger’s `csvColumns` configuration.

## Battery Guard

When configured with a MAX17048 driver, the logger emits `battery_low` and
`battery_ok` meta records to inform upstream systems that logging paused or
resumed. Adjust thresholds in `miniFlashDataBase_v2_0.ino` before deployment.

## Persistence Helpers

- `saveConfigToNVS(cfg, namespace)` / `loadConfigFromNVS(cfg, namespace)` –
  snapshot configuration for post-update boots.
- Factory information (model, flash model, device id) is stored in the last
  sector and survives factory resets.

Refer to `docs/cheat-sheet.md` for a quick-reference command/API map.
