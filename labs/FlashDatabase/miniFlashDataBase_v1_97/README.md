# miniFlashDataBase v1.97

Focus for this milestone:

- [x] **Transactional GC** – mark sectors with an “erase intent” before cleaning
  so power loss leaves state recoverable.
- [x] **RTC fallback** – persist the last good timestamp in NVS and refuse to log
  if the DS3231 is absent or reports time moving backwards.
- [x] **Help command** – extend the serial shell with a `help` verb covering all
  available commands.

Supporting tasks:

1. Add tests/harness coverage for the transactional GC logic.
2. Simulate RTC failure (missing module / time reset) and verify the fallback.
3. Update docs once changes are in place.
