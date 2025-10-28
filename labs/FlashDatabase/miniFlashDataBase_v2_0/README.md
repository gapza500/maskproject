# miniFlashDataBase v2.0 (Release Candidate)

This folder gathers everything required to ship miniFlashDataBase v2.0.

## Release Checklist

- - [x] Release notes covering v1.0 → v2.0 evolution (see `docs/release-notes.md`).
- [x] Documentation set assembled:
  - [`docs/getting-started.md`](docs/getting-started.md)
  - [`docs/api-reference.md`](docs/api-reference.md)
  - [`docs/storage-model.md`](docs/storage-model.md)
  - [`docs/sync-guide.md`](docs/sync-guide.md)
  - [`docs/troubleshooting.md`](docs/troubleshooting.md)
  - [`docs/performance.md`](docs/performance.md)
  - [`docs/cheat-sheet.md`](docs/cheat-sheet.md)
- [ ] Example suite coverage:
  - [x] Minimal logger (`miniFlashDataBase_v2_0.ino`)
  - [x] Sensor ingestion (SEN66 checker: `../../exampleUsagesen66/exampleUsagesen66.ino`)
  - [x] Predicate/export regression (`../miniFlashDataBase_v1_99_tests/`)
  - [x] Pagination tokens (`../miniFlashDataBase_v1_96_tests/`)
  - [x] Upload helpers (`examples/upload_http.cpp`, `examples/upload_mqtt.cpp`)
  - [x] CSV chart walkthrough (see `../miniFlashDataBase_v1_92/tests/query_demo_v1_92.ino` for CSV export)
  - [x] Shell/GC walkthrough (`docs/shell-tour.md`)
  - [x] AirMonitor sync story (`apps/flashlogger/examples/airmonitor_sync.ino`)
- [ ] Testing evidence captured:
  - Serial log for `miniFlashDataBase_v1_96_tests.ino`
  - Serial log for `miniFlashDataBase_v1_99_tests.ino`
  - Battery guard scenario from `miniFlashDataBase_v2_0.ino`
- [x] Package metadata: prepare `library.properties`, `keywords.txt`, and
  Arduino Library submission notes.
- [ ] Deployment checklist for mobile/cloud (BLE GATT stub, HTTP, MQTT).

Once everything is checked, tag `v2.0.0`, publish the docs/examples, and cut the
release.

## Quick Links

| Item | Location |
| --- | --- |
| Production sketch | `miniFlashDataBase_v2_0.ino` |
| Upload helper examples | `examples/upload_http.cpp`, `examples/upload_mqtt.cpp` |
| Predicate regression harness | `../miniFlashDataBase_v1_99_tests/miniFlashDataBase_v1_99_tests.ino` |
| Pagination regression harness | `../miniFlashDataBase_v1_96_tests/miniFlashDataBase_v1_96_tests.ino` |
| Documentation bundle | `docs/` |
| Release notes draft | `docs/release-notes.md` |
| Packaging TODOs | `NOTES.md` |
| Captured logs | `docs/logs/` |
| Shell tour transcript | `docs/shell-tour.md` |

## Test Artifacts

Attach or link the latest Serial transcripts when closing out the checklist:

- Predicate export run (v1.99 harness) showing `[upload] seq=...`.
- Pagination token walk-through (v1.96 harness).
- Battery guard log from the production sketch.

These act as acceptance evidence for the release.
