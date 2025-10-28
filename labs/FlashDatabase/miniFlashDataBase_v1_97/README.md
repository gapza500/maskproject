# miniFlashDataBase v1.97

Focus for this milestone:

- [x] **Transactional GC** – mark sectors with an “erase intent” before cleaning
  so power loss leaves state recoverable.
- [x] **RTC fallback** – persist the last good timestamp in NVS and refuse to log
  if the DS3231 is absent or reports time moving backwards.
- [x] **Help command** – extend the serial shell with a `help` verb covering all
  available commands.

## Use

1. Flash `tests/gc_rtc_demo_v1_97.ino` (defaults target the Cytron Maker Feather
   AIoT S3 pinout; adjust the board namespace if you bring your own wiring).
2. Open Serial @115200. A healthy run prints:
   - `[test] help command` followed by the unified command roster.
   - `[test] RTC fallback (backwards time)` showing a blocked append while the
     clock is rewound, then a successful append once time recovers.
   - `[test] transactional GC` with `ls` output before/after and an `🧹 GC`
     block erasing the pushed, out-of-retention day.
   - Final `[test] complete` marker.

If the RTC module is disconnected, the sketch will park after the initial
warning—reconnect the DS3231 and reset the board to continue.

## Notes

- The logger now persists the last good Unix timestamp (`rtc_last` in NVS) and
  refuses to log until time moves forward again. The harness rewinds the RTC by
  an hour to exercise the guardrail.
- Garbage collection writes an “erase intent” bit in each sector header before
  issuing the flash erase command. On reboot any sector left in that state is
  automatically reclaimed.
- `help` centralises shell documentation so mobile/CLI tooling can stay in sync
  without duplicating command lists.
