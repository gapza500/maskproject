# miniFlashDataBase v1.2

Adds RTC-based day rollover and formatted print per day.

## Use

1. Upload `sanity_v1_2.ino` (ensure DS3231 on SDA19/SCL20).
2. Serial @115200 should show:
   - `v1.2 sanity`
   - Three sample entries grouped by day via `printFormattedLogs()`.
   - `running gc...` message (GC is no-op unless sectors are marked pushed).

## Notes
- Day selection derived from RTC dayID.
- GC removes data older than 7 days when marked pushed.
