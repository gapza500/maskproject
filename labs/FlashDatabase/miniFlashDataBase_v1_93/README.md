# miniFlashDataBase v1.93

Sync cursor milestone: resumable exports plus day-level push markers.

## Use

1. Flash `tests/cursor_export_demo_v1_93.ino` to the target board (ESP32-S3/C6 +
   W25Q128, DS3231 on pins SDA=42/SCL=41 by default).
2. Open Serial Monitor @115200. A clean boot should show:
   - `[factory] reset OK` followed by five appended JSON samples.
   - `[cursor] write-head …` line printing the current SyncCursor snapshot.
   - `[export] full scan` block with three payloads emitted via `exportSince`.
   - `[shell] cursor clear/export` section exercising the shell handlers.
   - `[mark] markDaysPushedUntil …` note and an updated `stats` line.

If any block is missing, re-run `factory reset` from the shell and repeat.

## Notes

- `SyncCursor` now captures day/sector/address/seq, letting cloud sync resume.
- `exportSince(cursor, maxRows, cb, user)` streams records in JSONL/CSV.
- Shell shortcuts: `cursor show|clear|set` and `export <N>` all use the shared
  cursor; tests demonstrate the integration points.
- `markDaysPushedUntil(dayID)` flips the `pushed` bit in sector headers so GC
  can safely reclaim older days once uploads finish.
