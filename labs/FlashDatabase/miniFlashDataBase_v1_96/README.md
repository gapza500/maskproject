# miniFlashDataBase v1.96

Working notes for the v1.96 milestone:

- **Pagination tokens** – extend `queryLogs`/`queryLatest`/`exportSince` to emit
  and accept opaque cursors so mobile/cloud clients can page results reliably
  (implemented with hex page tokens and shell support).
- **`latest N` acceleration** – add a reverse scan path that walks sectors from
  newest to oldest, respecting abort conditions after `N` valid records (done).
- **Cursor persistence** – automatically save the export cursor to NVS whenever
  a streaming export finishes without interruption.

Supporting tasks:

1. Add regression tests covering pagination tokens and reverse scans.
2. Update the serial shell (`export`/`q latest`) to surface pagination options.
3. Document the new APIs, tokens, and cursor auto-save behaviour in the project README before tagging v1.96.

Test harness lives in `labs/FlashDatabase/miniFlashDataBase_v1_96_tests/` so it can be
built and flashed independently of the production sketch.
