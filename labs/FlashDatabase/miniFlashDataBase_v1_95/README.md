# miniFlashDataBase v1.95

Cursor persistence, anchor index rebuild, and flash diagnostics.

## Use

1. Flash `tests/cursor_nvs_demo_v1_95.ino` (board defaults assume the Cytron
   Maker Feather AIoT S3 wiring; adjust constants if you use another target).
2. Open Serial @115200. Expected flow:
   - `[cfg] begin(cfg) -> OK` then `[factory] reset` confirming a clean slate.
   - `[append]` lines for seeded records, followed by `[cursor] clear/export`.
   - `[nvs] saved` + `[nvs] loaded …` showing the persisted cursor snapshot.
   - `[rescan] rescanAndRefresh` message and a fresh `stats` summary.
   - `[scanbad]` output (usually reports zero quarantined sectors on healthy
     flash).

Run the sketch twice to confirm the cursor reload picks up where the previous
run left off.

## Notes

- `saveCursorNVS(ns,key)` and `loadCursorNVS(ns,key)` wrap ESP32 `Preferences`
  so exports can resume after brown-outs without custom storage glue.
- `rescanAndRefresh(rebuildSummaries, keepSelection)` rebuilds indexes, anchors,
  and optionally restores `cd day`/`cd sector` selections in one pass.
- `scanBadAndQuarantine()` (shell command `scanbad`) aggressively verifies each
  sector and quarantines anything that fails erase/write checks.
- Anchor metadata accelerates range queries by skipping sectors that cannot
  contain timestamps in the requested window; keep it fresh via rescan.
