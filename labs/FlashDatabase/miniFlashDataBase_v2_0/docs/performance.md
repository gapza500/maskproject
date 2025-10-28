# Performance Notes

- **Append Throughput**: With an 8 MHz SPI clock the logger sustains ~80 records
  per second (64-byte payloads). Increase `spi_clock_hz` up to 40 MHz if signal
  integrity allows.
- **Query Speed**: Anchors reduce range scans to O(number of matching sectors).
  Rebuild anchors via `buildSummaries()` or `rescanAndRefresh(...)` after bulk
  operations.
- **Pagination Tokens**: Tokens encode sector/offset and CRC for integrity.
  Parsing is O(1) and avoids rescanning from the start.
- **GC Cadence**: Run `gc()` after pushing each day or at startup. The intent
  markers guarantee safe recovery even if power fails mid-erase.
- **NVS Writes**: Cursor/config persistence uses ESP32 `Preferences`. Batch
  writes where possible to minimize flash wear.
- **Battery Guard**: When enabled, appends pause once SOC drops below the
  threshold. Resume automatically when `battery_ok` is emitted.

EOF
