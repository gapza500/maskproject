# miniFlashDataBase v1.0

Core append-only logger for external SPI NOR flash.

## Use

1. Upload `sanity_v1.0.ino`.
2. Open Serial @115200.
3. Expected output:
   - `Logger ready. Day ...` message.
   - Sector list showing day → sector mapping.
   - Raw log dump with the three appended JSON lines.

## Notes
- No shell, RTC, or safety features yet.
- DayID derived from `millis()` (placeholder).
