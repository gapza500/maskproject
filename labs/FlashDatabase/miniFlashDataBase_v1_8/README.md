# miniFlashDataBase v1.8

Safety improvements: generation-aware headers, wear leveling hints, quarantine list.

## Use

1. Upload `tests/sanity_v1_8.ino`.
2. Serial should show formatted logs, stats (`isLowSpace`), factory info, and GC run.

## Notes
- Sector header includes generation (tracked via factory bootCounter).
- Quarantine bad sectors automatically when operations fail.
- Resume logic handles power loss between writes.
