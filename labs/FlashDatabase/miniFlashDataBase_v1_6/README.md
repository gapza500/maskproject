# miniFlashDataBase v1.6

Adds interactive serial commands (pf/stats/factory/reset/gc) on top of v1.4.

## Use

1. Upload `tests/sanity_v1_6.ino`.
2. Serial @115200, type commands:
   - `pf` → formatted logs
   - `stats` → flash stats
   - `factory` → factory info
   - `mark` → mark current day pushed
   - `gc` → garbage collect
   - `reset <12digit>` → factory reset (keeps factory info)
