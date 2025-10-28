# Cheat Sheet

Command/API quick reference.

## Serial Shell

| Command | Description |
| --- | --- |
| `help` | List commands |
| `ls` | List days |
| `ls sectors [day]` | List sectors for a day |
| `cd day <date|id|#idx>` | Select day |
| `cd sector <id|#idx>` | Select sector |
| `print` / `info` | Dump selected data/info |
| `q latest <N>` | Query latest N records |
| `q day <YYYY-MM-DD>` | Query a day |
| `q range <A..B>` | Query date range |
| `fmt csv|jsonl` | Set output format |
| `set csv <cols>` | Configure CSV columns |
| `cursor show|clear|set|save|load` | Manage cursors |
| `export <N>` | Stream from cursor |
| `stats` | Show flash usage/health |
| `factory` | Print factory info |
| `gc` | Garbage collect |
| `scanbad` | Scan and quarantine sectors |
| `reset <code>` | Factory reset data |

## Key Functions

```cpp
logger.append(payload);
logger.queryLogs(spec, callback, user);
logger.exportSince(cursor, maxRows, callback, user, &nextToken);
logger.exportSinceWithMeta(cursor, maxRows, onRecord, user, &filter, &nextToken);
logger.markDaysPushedUntil(dayID);
logger.gc();
logger.saveCursorNVS("flog", "cursor");
logger.loadCursorNVS("flog", "cursor");
```

## Upload Helpers

```cpp
FlashLoggerUploadPolicy pol{.maxAttempts = 3,
                            .initialBackoffMs = 500,
                            .backoffMultiplier = 2.0f};
flashlogger_upload_ndjson(logger, cursor, batchSize, sender, user, pol, &nextToken);
flashlogger_upload_csv   (logger, cursor, batchSize, sender, user, pol, &nextToken);
```

## Useful Constants

| Name | Value | Notes |
| --- | --- | --- |
| `SECTOR_SIZE` | 4096 | SPI flash sector size |
| `MAX_SECTORS` | 4096 | Works with 16 MB W25Q128 |
| `REC_COMMIT` | `0xA5` | Commit marker byte |
| `HEADER_INTENT_ERASE` | `0xA5` | GC erase intent flag |

EOF
