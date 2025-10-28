# v1.99 Test Harness

Upload `miniFlashDataBase_v1_99_tests.ino` to verify predicate filters,
filtered exports, and anchor persistence.

Expected Serial flow:
1. `[append]` lines stay silent (success) as five samples are logged.
2. `runPredicateTest` prints JSON where `bat<=20`.
3. `runExportTest` emits `[upload] seq=...` records (`temp>=25`).
4. After `rescanAndRefresh` the same predicate/export repeats, proving anchors
   and cursor state survive a restart.

If appends fail with an RTC warning, check the DS3231 wiring and battery—logger
won’t write until timestamps move forward.
