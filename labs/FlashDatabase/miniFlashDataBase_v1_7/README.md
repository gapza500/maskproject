# miniFlashDataBase v1.7

New record header (len/crc/ts/seq) + commit marker for integrity.

## Use

1. Upload `tests/sanity_v1_7.ino`.
2. Serial should show:
   - Formatted logs (ISO style).
   - Updated stats with `isLowSpace()`.
   - Factory info.
   - Day marked pushed + GC run.

## Notes
- Records include CRC16 and commit byte 0xA5 (partial writes ignored).
- GC still requires `mark...[pushed]` + 7-day retention.
