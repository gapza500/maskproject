# miniFlashDataBase v1.99

Goals:

- [x] **Query predicates** – allow basic comparisons (`field<=value`, `>`, `!=`) in
  `queryLogs`/`export` so servers can prefilter before download.
- [x] **Persisted anchors** – store day/sector anchors across boots and rebuild
  lazily, reducing startup scans.
- [x] **Docs/tests** – extend examples to cover predicate usage and ensure anchors
  survive power cycles (see `miniFlashDataBase_v1_99_tests`).
- [x] **Battery guard** – pause logging when MAX17048 reports SOC below the
  threshold and emit `battery_low` / `battery_ok` events.

Before finalizing, rerun the v1.96 test harness and capture shell output (`help`,
`gc`, `export 2 token=...`) to confirm no regressions.

## Use

- `miniFlashDataBase_v1_99_tests/miniFlashDataBase_v1_99_tests.ino` exercises the
  predicate engine, `exportSinceWithMeta` filters, and anchor persistence. Flash
  it to confirm:
  1. Initial append + predicate filter logs (`bat<=20`).
  2. Upload helper emits `[upload]` lines for `temp>=25` records.
  3. After `rescanAndRefresh()` the same predicate still walks the cached anchors.
- Main sketch `miniFlashDataBase_v1_99.ino` drives the MAX17048 battery guard.
  For bench testing you can simulate low SOC by forcing the guard threshold in
  code; expect `battery_low` events and blocked appends until SOC recovers.

