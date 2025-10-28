# miniFlashDataBase v1.4

Adds factory info, RTC formatting, stats.

## Use

1. Upload `sanity_v1_4.ino`.
2. Serial output should show:
   - `v1.4 sanity`
   - Formatted daily logs (ISO dates, auto newline).
   - Flash stats (total/used/free/health/estimated days).
   - Factory info block.

## Notes
- `setFactoryInfo()` stores binary metadata in last sector.
- `printFactoryInfo()` prints current metadata.
- `setDateStyle()` chooses Thai/ISO/US formats.
- GC and factory reset commands available but commented.
