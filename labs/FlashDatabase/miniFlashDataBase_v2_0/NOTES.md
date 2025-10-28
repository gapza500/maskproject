# Packaging & Release Notes TODO

Use this page to coordinate the remaining release work.

## Regression Artifacts

- [ ] Capture Serial log for `miniFlashDataBase_v1_96_tests/miniFlashDataBase_v1_96_tests.ino` (saved to `docs/logs/v1_96_pagination.log`)
      demonstrating pagination tokens.
- [x] Capture Serial log for `miniFlashDataBase_v1_99_tests/miniFlashDataBase_v1_99_tests.ino` (see `docs/logs/v1_99_predicate_export.log`)
      showing predicate export and restart replay.
- [ ] Record battery guard event from `miniFlashDataBase_v2_0.ino` (simulated log in `docs/logs/battery_guard.log`) (simulate low SOC).

Store the logs alongside the release (e.g. attach to GitHub release or drop into
`docs/logs/`).

## Packaging Checklist

- [ ] Create `library.properties` with name, version `2.0.0`, author, maintainer,
      paragraph, category, url, architectures (`esp32`).
- [ ] Populate `keywords.txt` (classes, functions, constants).
- [ ] Ensure `src/` layout mirrors Arduino library expectations (move public
      headers if necessary or add `library.json` for PlatformIO).
- [ ] Update root README with installation instructions and link to docs bundle.
- [ ] Prepare changelog snippet for GitHub release (`docs/release-notes.md`).
- [ ] Optional: generate Arduino Library Manager submission PR template.

## Deployment Checklist (Mobile/Cloud)

- [ ] BLE GATT stub: characteristic for NDJSON payloads + acknowledgement.
- [ ] HTTP endpoint contract: expected headers, response codes, retry semantics.
- [ ] MQTT topic structure and QoS recommendation.
- [ ] Note on resetting `rtc_last` during manufacturing tests.
- [ ] Document OTA update flow (config persistence, cursor retention).

Update this file as tasks complete; once everything is checked, tag `v2.0.0`.
