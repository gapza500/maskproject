# Troubleshooting

## Append Fails with RTC Warning

- Ensure the DS3231 is wired correctly and has a backup battery.
- Clear the stored `rtc_last` key: use `Preferences` (`flog` namespace) to set
  the value to `0` before starting controlled tests.
- Confirm `rtc.now().unixtime()` increases monotonically.

## No Output from Queries

- Run `rescanAndRefresh(true, false)` after a factory reset or power loss.
- Check if the predicate filters are too restrictive.
- Use `shell > ls` and `ls sectors` to inspect day/sector state.

## GC Skips Sectors

- Sectors must be marked `pushed` before GC will erase them. Call
  `markDaysPushedUntil(dayID)`.
- The transactional GC writes an intent flag; if verification fails it
  quarantines the sector. Check serial logs for `Erase verify FAILED...`.

## Battery Guard Blocking Appends

- Verify MAX17048 wiring and threshold configuration.
- Emit a `battery_ok` event (call the helper) once voltage recovers.

## Upload Retries Forever

- Inspect the sender callback return value—return `true` after a successful
  transmission.
- Review backoff settings; set `maxAttempts` > 0 and
  `backoffMultiplier >= 1.0`.

## Shell Not Responding

- Confirm the sketch includes `enableShell = true`.
- Avoid blocking calls in `loop()`. Use `logger.handleCommand(line, Serial)` to
  process buffered input.

## Reset Code Fails

- Factory reset requires the 12-digit code in config. Update
  `cfg.resetCode12` if you changed the default.

EOF
