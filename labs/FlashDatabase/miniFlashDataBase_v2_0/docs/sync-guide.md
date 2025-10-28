# Sync Guide

This guide explains how to move data from miniFlashDataBase into cloud/mobile
systems using the built-in cursor and upload helpers.

## Workflow Overview

1. **Collect data** locally using `logger.append(...)`.
2. **Mark pushed days** when uploads succeed (`markDayPushed` or shell
   `markDaysPushedUntil`).
3. **Export with cursors** to support resumable transfers.
4. **Persist state** (cursor + config) so power loss does not result in duplicate
   uploads.

## Managing Cursors

```cpp
SyncCursor cursor;
if (!logger.loadCursorNVS("flog", "cursor")) {
  logger.clearCursor(); // start from earliest record
}
```

After a successful export:

```cpp
logger.saveCursorNVS("flog", "cursor");
```

The serial shell mirrors these calls (`cursor save`, `cursor load`).

## Filtering Data Before Upload

Use `QuerySpec` predicates to reduce payload size:

```cpp
QuerySpec filter;
filter.predicates[0] = {"bat", PRED_GE, 20.0f};
filter.predicateCount = 1;
```

Pass the filter into `exportSinceWithMeta` or the upload helpers.

## Upload Helpers

The helper layer wraps retry/backoff logic and adds an idempotency key based on
`RecordHeader::seq`:

```cpp
FlashLoggerUploadPolicy policy;
policy.maxAttempts = 3;
policy.initialBackoffMs = 500;
policy.backoffMultiplier = 2.0f;

String nextToken;
flashlogger_upload_ndjson(logger, cursor, 128, sender, user, policy, &nextToken);
```

Implement `sender` to push the payload to HTTP/MQTT/BLE transports. Return
`false` to request a retry; the helper waits using exponential backoff.

## Pagination Tokens

Both `queryLogs` and the upload helpers can emit pagination tokens. Persist the
`nextToken` alongside your cursor to resume mid-page after a disruption.

## Handling Power Loss

- Persist `SyncCursor` after each successful export.
- Ensure the RTC keeps moving forward (battery-backed DS3231).
- Optionally persist `FlashLoggerConfig` so pin assignments survive OTA updates.

## Mobile Integrations

- **BLE**: map the sender callback to a GATT characteristic write. Maintain a
  ring buffer on the phone to acknowledge batches.
- **MQTT**: publish NDJSON lines to a topic. Use the sequence number as the
  MQTT message ID for deduplication.
- **HTTP**: POST NDJSON batches. If the server responds with a transient error,
  return `false` so the helper retries.

## Cleanup

After the server confirms older days are safely processed, call
`markDaysPushedUntil(dayID)` and run `gc()` periodically to reclaim flash.

EOF
