# Storage Model

miniFlashDataBase uses an append-only log spanning an external SPI NOR flash.
This document summarizes how records are laid out and how the library maintains
integrity across resets.

## Physical Layout

- Flash is divided into 4 KB sectors (`SECTOR_SIZE`).
- The final sector is reserved for factory metadata (`FactoryInfo`).
- Remaining sectors store log data with a 16-byte `SectorHeader` followed by
  variable-length records.

```
+-----------+ 0x000000: SectorHeader (magic, dayID, pushed, generation)
| Header    |
+-----------+ 0x000010: RecordHeader (len, crc, ts, seq, flags, rsv)
| Record 0  |
+-----------+ payload bytes
| ...       |
+-----------+
```

## Day Buckets

Each record belongs to a `dayID` (days since 2000-01-01). The logger groups
sectors by day:

- When a new record crosses midnight, `FlashLogger` selects/creates a sector for
  the new day.
- `markDayPushed(dayID)` toggles the `pushed` flag in the sector header to mark
  the day as safely uploaded.

## Record Integrity

Every append writes three stages:

1. `RecordHeader`
2. Payload JSON (with trailing `
`)
3. Commit byte (`0xA5`)

If power is lost mid-write, the next boot detects the missing commit and ignores
partial data. During reads the library recomputes `crc16` to verify payloads.

## Anchors

Version 1.95+ maintains in-RAM anchor structures and, in v1.99, persists them to
flash so range queries can skip sectors outside the requested time window. A
rescan lazily rebuilds anchors if the persisted data is missing or stale.

## Factory Metadata

`FactoryInfo` contains:

- `model`, `flashModel`, `deviceId`
- `firstDayID`
- `defaultDateStyle`
- `totalEraseOps`, `bootCounter`
- `badCount`

`factoryReset(code)` erases user data but keeps this block.

## Garbage Collection

`gc()` implements a transactional erase:

1. Write an “erase intent” marker to the sector header.
2. Erase the sector.
3. Verify; on failure, quarantine the sector.

On boot any sector left with the intent flag is automatically reclaimed before
resuming normal operations.

## RTC Safeguard

To avoid corrupt ordering after brown-outs, the logger remembers the last good
Unix timestamp (`rtc_last` in NVS). Appends are rejected if the RTC reports a
value before this baseline. Tests should reset the key (e.g. using `Preferences`)
when running deterministic harnesses.
